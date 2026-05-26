#include "account.h"
#include "audit_logger.h"
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <iomanip>

namespace IBS {

// ============================================================
//  PIN hashing — FNV-1a + salt (not crypto-grade but obfuscated)
// ============================================================
std::string Account::hash_pin(const std::string& raw_pin) {
    // FNV-1a with a fixed salt
    const std::string salt = "IBS_SALT_2024_$#@!";
    std::string salted = salt + raw_pin + salt;
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : salted) {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    std::ostringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << hash;
    return ss.str();
}

Account::Account(const AccountID&  id,
                 const CustomerID& owner_id,
                 AccountType       type,
                 Money             initial_balance)
    : id_(id), owner_id_(owner_id), type_(type), balance_(initial_balance)
{
    if (initial_balance.cents > 0) {
        auto t = Transaction::create(TransactionType::DEPOSIT, "", id_,
                                     initial_balance, "Initial deposit");
        t.mark_completed();
        history_.push_back(std::move(t));
    }
}

Money Account::balance() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return balance_;
}

void Account::push_transaction(Transaction&& t) {
    history_.push_back(std::move(t));
}

bool Account::deposit(Money amount, const std::string& note) {
    if (amount.cents <= 0) throw std::invalid_argument("Deposit amount must be positive");
    std::lock_guard<std::mutex> lk(mtx_);
    if (status_ != AccountStatus::ACTIVE)
        throw std::runtime_error("Account is not active");

    auto t = Transaction::create(TransactionType::DEPOSIT, "", id_, amount, note);
    balance_ = balance_ + amount;
    t.mark_completed();
    push_transaction(std::move(t));
    AUDIT_TXN(id_, "DEPOSIT", amount.to_string() + " | " + note);
    return true;
}

bool Account::withdraw(Money amount, const std::string& note) {
    if (amount.cents <= 0) throw std::invalid_argument("Withdrawal amount must be positive");
    std::lock_guard<std::mutex> lk(mtx_);
    if (status_ == AccountStatus::LOCKED)
        throw std::runtime_error("Account is locked");
    if (status_ != AccountStatus::ACTIVE)
        throw std::runtime_error("Account is not active");
    if (balance_ < amount)
        throw std::runtime_error("Insufficient funds: balance=" +
                                  balance_.to_string() + " requested=" + amount.to_string());

    auto t = Transaction::create(TransactionType::WITHDRAWAL, id_, "", amount, note);
    balance_ = balance_ - amount;
    t.mark_completed();
    push_transaction(std::move(t));
    AUDIT_TXN(id_, "WITHDRAW", amount.to_string() + " | " + note);
    return true;
}

bool Account::transfer_to(Account& dest, Money amount, const std::string& note) {
    if (&dest == this) throw std::invalid_argument("Cannot transfer to same account");
    if (amount.cents <= 0) throw std::invalid_argument("Transfer amount must be positive");

    // Lock both accounts in a consistent order to avoid deadlock
    std::mutex* first  = (this < &dest) ? &mtx_ : &dest.mtx_;
    std::mutex* second = (this < &dest) ? &dest.mtx_ : &mtx_;
    std::unique_lock<std::mutex> lk1(*first,  std::defer_lock);
    std::unique_lock<std::mutex> lk2(*second, std::defer_lock);
    std::lock(lk1, lk2);

    if (status_ != AccountStatus::ACTIVE)
        throw std::runtime_error("Source account is not active");
    if (dest.status_ != AccountStatus::ACTIVE)
        throw std::runtime_error("Destination account is not active");
    if (balance_ < amount)
        throw std::runtime_error("Insufficient funds");

    auto t_out = Transaction::create(TransactionType::TRANSFER, id_, dest.id_, amount, note);
    auto t_in  = Transaction::create(TransactionType::TRANSFER, id_, dest.id_, amount, note);

    balance_      = balance_      - amount;
    dest.balance_ = dest.balance_ + amount;

    t_out.mark_completed(); t_in.mark_completed();
    push_transaction(std::move(t_out));
    dest.push_transaction(std::move(t_in));

    AUDIT_TXN(id_, "TRANSFER OUT", amount.to_string() + " -> " + dest.id_);
    AUDIT_TXN(dest.id_, "TRANSFER IN", amount.to_string() + " <- " + id_);
    return true;
}

std::vector<Transaction> Account::recent_history(size_t n) const {
    std::lock_guard<std::mutex> lk(mtx_);
    if (history_.size() <= n) return history_;
    return std::vector<Transaction>(history_.end() - n, history_.end());
}

bool Account::verify_pin(const std::string& raw_pin) const {
    return hash_pin(raw_pin) == pin_hash_;
}

void Account::set_pin(const std::string& raw_pin) {
    if (raw_pin.size() < 4) throw std::invalid_argument("PIN must be at least 4 digits");
    pin_hash_ = hash_pin(raw_pin);
}

void Account::record_failed_login() {
    ++failed_logins_;
    if (failed_logins_ >= 3) {
        status_ = AccountStatus::LOCKED;
        AUDIT_SECURITY(id_, "ACCOUNT LOCKED", "3 failed login attempts");
    }
}

bool Account::apply_interest() {
    std::lock_guard<std::mutex> lk(mtx_);
    if (status_ != AccountStatus::ACTIVE) return false;
    if (type_ != AccountType::SAVINGS && type_ != AccountType::FIXED_DEPOSIT)
        return false;

    int64_t interest_cents = static_cast<int64_t>(balance_.cents * interest_rate_ / 12.0);
    Money interest(interest_cents);
    if (interest.cents == 0) return false;

    balance_ = balance_ + interest;
    auto t = Transaction::create(TransactionType::INTEREST, "", id_, interest,
                                 "Monthly interest @ " + std::to_string(interest_rate_ * 100) + "%");
    t.mark_completed();
    push_transaction(std::move(t));
    AUDIT_TXN(id_, "INTEREST APPLIED", interest.to_string());
    return true;
}

std::string Account::type_str() const {
    switch (type_) {
        case AccountType::SAVINGS:       return "SAVINGS";
        case AccountType::CHECKING:      return "CHECKING";
        case AccountType::FIXED_DEPOSIT: return "FIXED_DEPOSIT";
    }
    return "UNKNOWN";
}

std::string Account::status_str() const {
    switch (status_) {
        case AccountStatus::ACTIVE:    return "ACTIVE";
        case AccountStatus::LOCKED:    return "LOCKED";
        case AccountStatus::CLOSED:    return "CLOSED";
        case AccountStatus::SUSPENDED: return "SUSPENDED";
    }
    return "UNKNOWN";
}

// ---- Serialization: section-separated blob ----
std::string Account::serialize() const {
    std::ostringstream ss;
    ss << "ACCOUNT_BEGIN\n";
    ss << id_ << "\n" << owner_id_ << "\n"
       << static_cast<int>(type_)   << "\n"
       << static_cast<int>(status_) << "\n"
       << balance_.cents << "\n"
       << pin_hash_ << "\n"
       << failed_logins_ << "\n"
       << interest_rate_ << "\n";
    ss << "TXN_BEGIN\n";
    for (const auto& t : history_)
        ss << t.serialize() << "\n";
    ss << "TXN_END\n";
    ss << "ACCOUNT_END\n";
    return ss.str();
}

Account Account::deserialize(const std::string& blob) {
    std::istringstream ss(blob);
    std::string line;

    auto nextline = [&]() {
        std::getline(ss, line);
        return line;
    };

    nextline(); // ACCOUNT_BEGIN
    Account a;
    a.id_           = nextline();
    a.owner_id_     = nextline();
    a.type_         = static_cast<AccountType>(std::stoi(nextline()));
    a.status_       = static_cast<AccountStatus>(std::stoi(nextline()));
    a.balance_      = Money(static_cast<int64_t>(std::stoll(nextline())));
    a.pin_hash_     = nextline();
    a.failed_logins_= std::stoi(nextline());
    a.interest_rate_= std::stod(nextline());

    nextline(); // TXN_BEGIN
    while (std::getline(ss, line) && line != "TXN_END") {
        if (!line.empty())
            a.history_.push_back(Transaction::deserialize(line));
    }
    // ACCOUNT_END consumed externally
    return a;
}

} // namespace IBS
