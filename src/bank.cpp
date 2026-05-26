#include "bank.h"
#include <sstream>
#include <stdexcept>
#include <iomanip>
#include <algorithm>
#include <iostream>
#include <filesystem>

namespace IBS {

std::atomic<uint64_t> Bank::cust_counter_{1};
std::atomic<uint64_t> Bank::acct_counter_{1};

// ============================================================
//  Constructor / Destructor
// ============================================================
Bank::Bank(const std::string& name, const std::string& data_dir)
    : name_(name), data_dir_(data_dir),
      account_index_(1 << 17),       // 128K initial capacity
      account_cache_(512)            // LRU: 512 hot accounts
{
    // Ensure data directory exists
    std::filesystem::create_directories(data_dir_);

    // Open audit log
    AuditLogger::instance().open(data_dir_ + "/audit.log");
    AUDIT_INFO("SYSTEM", "Bank started", name_);

    // Launch thread pool
    for (int i = 0; i < THREAD_POOL_SIZE; ++i)
        workers_.emplace_back([this]{ worker_loop(); });
}

Bank::~Bank() {
    stop_workers_.store(true);
    queue_cv_.notify_all();
    for (auto& t : workers_) if (t.joinable()) t.join();
    AUDIT_INFO("SYSTEM", "Bank shutdown", name_);
}

// ============================================================
//  Worker thread loop
// ============================================================
void Bank::worker_loop() {
    while (true) {
        TransactionRequest req;
        {
            std::unique_lock<std::mutex> lk(queue_mtx_);
            queue_cv_.wait(lk, [this]{
                return !tx_queue_.empty() || stop_workers_.load();
            });
            if (stop_workers_.load() && tx_queue_.empty()) return;
            req = std::move(tx_queue_.front());
            tx_queue_.pop();
        }
        process_request(req);
        --pending_count_;
    }
}

void Bank::process_request(TransactionRequest& req) {
    try {
        bool ok = false;
        switch (req.type) {
            case TransactionType::DEPOSIT:
                ok = deposit(req.from.empty() ? req.to : req.from, req.amount, req.note);
                break;
            case TransactionType::WITHDRAWAL:
                ok = withdraw(req.from, req.amount, "", req.note);
                break;
            case TransactionType::TRANSFER:
                ok = transfer(req.from, req.to, req.amount, "", req.note);
                break;
            default: break;
        }
        if (req.callback) req.callback(ok, ok ? "OK" : "Failed");
    } catch (const std::exception& e) {
        if (req.callback) req.callback(false, e.what());
    }
}

// ============================================================
//  ID generation
// ============================================================
std::string Bank::gen_customer_id() {
    uint64_t n = cust_counter_.fetch_add(1, std::memory_order_relaxed);
    std::ostringstream ss;
    ss << "CUST" << std::setfill('0') << std::setw(8) << n;
    return ss.str();
}

std::string Bank::gen_account_id(AccountType t) {
    uint64_t n = acct_counter_.fetch_add(1, std::memory_order_relaxed);
    std::string prefix;
    switch (t) {
        case AccountType::SAVINGS:       prefix = "SAV"; break;
        case AccountType::CHECKING:      prefix = "CHK"; break;
        case AccountType::FIXED_DEPOSIT: prefix = "FD";  break;
    }
    std::ostringstream ss;
    ss << prefix << std::setfill('0') << std::setw(9) << n;
    return ss.str();
}

// ============================================================
//  Customer CRUD
// ============================================================
CustomerID Bank::create_customer(const std::string& first, const std::string& last,
                                  const std::string& email, const std::string& phone,
                                  const std::string& address) {
    std::lock_guard<std::mutex> lk(bank_mtx_);
    auto cid = gen_customer_id();
    auto c = std::make_shared<Customer>(cid, first, last, email, phone, address);
    customers_[cid] = c;
    AUDIT_INFO(cid, "CUSTOMER CREATED", first + " " + last + " | " + email);
    return cid;
}

bool Bank::delete_customer(const CustomerID& cid) {
    std::lock_guard<std::mutex> lk(bank_mtx_);
    auto it = customers_.find(cid);
    if (it == customers_.end()) return false;
    // Remove all accounts from index
    for (const auto& a : it->second->accounts()) {
        account_index_.remove(a->id());
        account_cache_.invalidate(a->id());
    }
    customers_.erase(it);
    AUDIT_WARN(cid, "CUSTOMER DELETED");
    return true;
}

std::shared_ptr<Customer> Bank::find_customer(const CustomerID& cid) const {
    std::lock_guard<std::mutex> lk(bank_mtx_);
    auto it = customers_.find(cid);
    return (it != customers_.end()) ? it->second : nullptr;
}

std::shared_ptr<Customer> Bank::search_customer_by_email(const std::string& email) const {
    std::lock_guard<std::mutex> lk(bank_mtx_);
    for (const auto& [_, c] : customers_)
        if (c->email() == email) return c;
    return nullptr;
}

std::vector<std::shared_ptr<Customer>> Bank::all_customers() const {
    std::lock_guard<std::mutex> lk(bank_mtx_);
    std::vector<std::shared_ptr<Customer>> v;
    v.reserve(customers_.size());
    for (const auto& [_, c] : customers_) v.push_back(c);
    return v;
}

// ============================================================
//  Account CRUD
// ============================================================
AccountID Bank::create_account(const CustomerID& cid, AccountType type,
                                Money initial_deposit, const std::string& raw_pin) {
    std::lock_guard<std::mutex> lk(bank_mtx_);
    auto it = customers_.find(cid);
    if (it == customers_.end())
        throw std::runtime_error("Customer not found: " + cid);

    auto aid = gen_account_id(type);
    auto acct = std::make_shared<Account>(aid, cid, type, initial_deposit);
    acct->set_pin(raw_pin);
    it->second->add_account(acct);
    account_index_.insert(aid, acct);
    account_cache_.put(aid, acct);

    AUDIT_INFO(aid, "ACCOUNT CREATED",
               "customer=" + cid + " type=" + acct->type_str() +
               " init=" + initial_deposit.to_string());
    return aid;
}

bool Bank::close_account(const AccountID& aid) {
    std::lock_guard<std::mutex> lk(bank_mtx_);
    auto acct = get_account_cached(aid);
    if (!acct) return false;
    acct->close();
    account_index_.remove(aid);
    account_cache_.invalidate(aid);
    AUDIT_WARN(aid, "ACCOUNT CLOSED");
    return true;
}

std::shared_ptr<Account> Bank::find_account(const AccountID& aid) {
    return get_account_cached(aid);
}

std::shared_ptr<Account> Bank::get_account_cached(const AccountID& aid) {
    // Check LRU cache first
    auto cached = account_cache_.get(aid);
    if (cached) return *cached;
    // Fall through to hash index
    auto found = account_index_.find(aid);
    if (found) {
        account_cache_.put(aid, *found);
        return *found;
    }
    return nullptr;
}

// ============================================================
//  Synchronous transactions
// ============================================================
bool Bank::deposit(const AccountID& aid, Money amount, const std::string& note) {
    auto acct = get_account_cached(aid);
    if (!acct) throw std::runtime_error("Account not found: " + aid);

    auto tx = Transaction::create(TransactionType::DEPOSIT, "", aid, amount, note);
    auto alerts = fraud_.evaluate(tx, acct->failed_logins());
    for (const auto& a : alerts) {
        std::cerr << a.to_string() << "\n";
        if (a.block_transaction) {
            AUDIT_SECURITY(aid, "DEPOSIT BLOCKED BY FRAUD", a.detail);
            return false;
        }
    }
    bool ok = acct->deposit(amount, note);
    if (ok) fraud_.record_transaction(tx);
    return ok;
}

bool Bank::withdraw(const AccountID& aid, Money amount, const std::string& pin,
                    const std::string& note) {
    auto acct = get_account_cached(aid);
    if (!acct) throw std::runtime_error("Account not found: " + aid);

    if (!pin.empty() && !acct->verify_pin(pin)) {
        acct->record_failed_login();
        AUDIT_SECURITY(aid, "WITHDRAWAL FAILED - BAD PIN",
                       "failed_logins=" + std::to_string(acct->failed_logins()));
        throw std::runtime_error("Invalid PIN");
    }
    if (!pin.empty()) acct->reset_failed_logins();

    auto tx = Transaction::create(TransactionType::WITHDRAWAL, aid, "", amount, note);
    auto alerts = fraud_.evaluate(tx, acct->failed_logins());
    for (const auto& a : alerts) {
        std::cerr << a.to_string() << "\n";
        if (a.block_transaction) {
            AUDIT_SECURITY(aid, "WITHDRAWAL BLOCKED BY FRAUD", a.detail);
            return false;
        }
    }

    // Save for potential undo
    bool ok = acct->withdraw(amount, note);
    if (ok) {
        undo_stacks_[aid].push(acct->history().back());
        fraud_.record_transaction(tx);
    }
    return ok;
}

bool Bank::transfer(const AccountID& from, const AccountID& to, Money amount,
                    const std::string& pin, const std::string& note) {
    auto src  = get_account_cached(from);
    auto dst  = get_account_cached(to);
    if (!src) throw std::runtime_error("Source account not found: " + from);
    if (!dst) throw std::runtime_error("Destination account not found: " + to);

    if (!pin.empty() && !src->verify_pin(pin)) {
        src->record_failed_login();
        AUDIT_SECURITY(from, "TRANSFER FAILED - BAD PIN");
        throw std::runtime_error("Invalid PIN");
    }
    if (!pin.empty()) src->reset_failed_logins();

    auto tx = Transaction::create(TransactionType::TRANSFER, from, to, amount, note);
    auto alerts = fraud_.evaluate(tx);
    for (const auto& a : alerts) {
        std::cerr << a.to_string() << "\n";
        if (a.block_transaction) {
            AUDIT_SECURITY(from, "TRANSFER BLOCKED BY FRAUD", a.detail);
            return false;
        }
    }

    bool ok = src->transfer_to(*dst, amount, note);
    if (ok) fraud_.record_transaction(tx);
    return ok;
}

void Bank::submit_transaction(TransactionRequest req) {
    ++pending_count_;
    {
        std::lock_guard<std::mutex> lk(queue_mtx_);
        tx_queue_.push(std::move(req));
    }
    queue_cv_.notify_one();
}

// ============================================================
//  Undo / Redo
// ============================================================
bool Bank::undo_last_transaction(const AccountID& aid) {
    auto& stk = undo_stacks_[aid];
    if (stk.empty()) return false;

    auto last = stk.top(); stk.pop();
    auto acct = get_account_cached(aid);
    if (!acct) return false;

    // Reverse the effect
    if (last.type() == TransactionType::WITHDRAWAL) {
        acct->deposit(last.amount(), "UNDO: " + last.id());
    } else if (last.type() == TransactionType::DEPOSIT) {
        try { acct->withdraw(last.amount(), "UNDO: " + last.id()); }
        catch (...) { return false; }
    }
    AUDIT_WARN(aid, "TRANSACTION UNDONE", last.id());
    return true;
}

// ============================================================
//  Interest Engine
// ============================================================
int Bank::apply_interest_to_all_savings() {
    int count = 0;
    std::lock_guard<std::mutex> lk(bank_mtx_);
    account_index_.for_each([&](const AccountID&, std::shared_ptr<Account> acct) {
        if (acct->apply_interest()) ++count;
    });
    AUDIT_INFO("SYSTEM", "INTEREST APPLIED", "accounts=" + std::to_string(count));
    return count;
}

// ============================================================
//  Security
// ============================================================
bool Bank::verify_pin(const AccountID& aid, const std::string& raw_pin) {
    auto acct = get_account_cached(aid);
    if (!acct) return false;
    return acct->verify_pin(raw_pin);
}

// ============================================================
//  Persistence
// ============================================================
bool Bank::save(const std::string& filename) {
    std::string path = filename.empty() ? data_dir_ + "/bank.dat" : filename;
    std::ofstream f(path, std::ios::trunc);
    if (!f) return false;

    std::lock_guard<std::mutex> lk(bank_mtx_);
    f << "BANK:" << name_ << "\n";
    f << "CUSTOMERS:" << customers_.size() << "\n";
    for (const auto& [_, c] : customers_)
        f << c->serialize();
    f.flush();
    AUDIT_INFO("SYSTEM", "DATABASE SAVED", path);
    return true;
}

bool Bank::load(const std::string& filename) {
    std::string path = filename.empty() ? data_dir_ + "/bank.dat" : filename;
    std::ifstream f(path);
    if (!f) return false;

    std::lock_guard<std::mutex> lk(bank_mtx_);
    customers_.clear();
    account_index_ = DoubleHashTable<AccountID, std::shared_ptr<Account>>(1 << 17);
    account_cache_.clear();

    std::string line;
    std::getline(f, line); // BANK:name
    name_ = line.substr(5);
    std::getline(f, line); // CUSTOMERS:N
    size_t n = std::stoul(line.substr(10));

    for (size_t i = 0; i < n; ++i) {
        std::ostringstream blob;
        while (std::getline(f, line)) {
            blob << line << "\n";
            if (line == "CUSTOMER_END") break;
        }
        auto c = std::make_shared<Customer>(Customer::deserialize(blob.str()));
        customers_[c->id()] = c;
        for (const auto& a : c->accounts()) {
            account_index_.insert(a->id(), a);
        }
    }
    AUDIT_INFO("SYSTEM", "DATABASE LOADED", path +
               " | customers=" + std::to_string(customers_.size()));
    return true;
}

// ============================================================
//  Reporting
// ============================================================
void Bank::print_summary(std::ostream& out) const {
    std::lock_guard<std::mutex> lk(bank_mtx_);
    out << "\n╔══════════════════════════════════════════════════════╗\n";
    out << "║              " << std::left << std::setw(39) << name_ << "║\n";
    out << "╠══════════════════════════════════════════════════════╣\n";
    out << "║  Total Customers : " << std::setw(33) << customers_.size() << "║\n";
    out << "║  Total Accounts  : " << std::setw(33) << account_index_.size() << "║\n";
    out << "║  Pending TXNs    : " << std::setw(33) << pending_count_.load() << "║\n";

    Money grand_total;
    for (const auto& [_, c] : customers_)
        grand_total = grand_total + c->total_balance();
    out << "║  Total Assets    : " << std::setw(33) << grand_total.to_string() << "║\n";
    out << "╚══════════════════════════════════════════════════════╝\n";
}

void Bank::print_hash_stats(std::ostream& out) const {
    account_index_.print_stats(out);
}

void Bank::print_cache_stats(std::ostream& out) const {
    out << "\n╔══════════════════════════════╗\n";
    out << "║     LRU Cache Statistics     ║\n";
    out << "╠══════════════════════════════╣\n";
    out << "║  Capacity  : " << std::setw(16) << account_cache_.capacity() << " ║\n";
    out << "║  Size      : " << std::setw(16) << account_cache_.size()     << " ║\n";
    out << "║  Hits      : " << std::setw(16) << account_cache_.hits()     << " ║\n";
    out << "║  Misses    : " << std::setw(16) << account_cache_.misses()   << " ║\n";
    out << "║  Evictions : " << std::setw(16) << account_cache_.evictions()<< " ║\n";
    out << std::fixed << std::setprecision(2);
    out << "║  Hit rate  : " << std::setw(15) << account_cache_.hit_rate()*100 << "% ║\n";
    out << "╚══════════════════════════════╝\n";
}

void Bank::print_fraud_report(std::ostream& out) const {
    fraud_.print_report(out);
}

void Bank::generate_full_report(std::ostream& out) const {
    print_summary(out);
    print_hash_stats(out);
    print_cache_stats(out);
    print_fraud_report(out);

    out << "\n--- Customer Details ---\n";
    std::lock_guard<std::mutex> lk(bank_mtx_);
    for (const auto& [_, c] : customers_) out << c->summary();
}

} // namespace IBS
