#include "customer.h"
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <chrono>

namespace IBS {

Customer::Customer(const CustomerID& id,
                   const std::string& first_name,
                   const std::string& last_name,
                   const std::string& email,
                   const std::string& phone,
                   const std::string& address)
    : id_(id), first_name_(first_name), last_name_(last_name),
      email_(email), phone_(phone), address_(address),
      created_at_(std::chrono::system_clock::now())
{}

void Customer::add_account(std::shared_ptr<Account> acc) {
    if (!acc) throw std::invalid_argument("Cannot add null account");
    for (const auto& a : accounts_)
        if (a->id() == acc->id())
            throw std::runtime_error("Account already added to customer");
    accounts_.push_back(std::move(acc));
}

bool Customer::remove_account(const AccountID& acc_id) {
    auto it = std::find_if(accounts_.begin(), accounts_.end(),
                           [&](const auto& a){ return a->id() == acc_id; });
    if (it == accounts_.end()) return false;
    accounts_.erase(it);
    return true;
}

std::shared_ptr<Account> Customer::get_account(const AccountID& acc_id) const {
    for (const auto& a : accounts_)
        if (a->id() == acc_id) return a;
    return nullptr;
}

Money Customer::total_balance() const {
    Money total;
    for (const auto& a : accounts_)
        if (a->status() == AccountStatus::ACTIVE)
            total = total + a->balance();
    return total;
}

std::string Customer::summary() const {
    std::ostringstream ss;
    ss << "┌──────────────────────────────────────────┐\n";
    ss << "│ Customer: " << std::left << std::setw(30) << full_name() << " │\n";
    ss << "│ ID      : " << std::setw(30) << id_                      << " │\n";
    ss << "│ Email   : " << std::setw(30) << email_                   << " │\n";
    ss << "│ Phone   : " << std::setw(30) << phone_                   << " │\n";
    ss << "│ Accounts: " << std::setw(30) << accounts_.size()         << " │\n";
    ss << "│ Net Worth: " << std::setw(29) << total_balance().to_string() << " │\n";
    ss << "└──────────────────────────────────────────┘\n";
    for (const auto& a : accounts_) {
        ss << "  Account: " << a->id()
           << "  Type: " << a->type_str()
           << "  Balance: " << a->balance().to_string()
           << "  Status: " << a->status_str() << "\n";
    }
    return ss.str();
}

std::string Customer::serialize() const {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  created_at_.time_since_epoch()).count();
    std::ostringstream ss;
    ss << "CUSTOMER_BEGIN\n";
    ss << id_ << "\n" << first_name_ << "\n" << last_name_ << "\n"
       << email_ << "\n" << phone_ << "\n" << address_ << "\n" << ms << "\n";
    ss << "ACCOUNTS:" << accounts_.size() << "\n";
    for (const auto& a : accounts_)
        ss << a->serialize();
    ss << "CUSTOMER_END\n";
    return ss.str();
}

Customer Customer::deserialize(const std::string& blob) {
    std::istringstream ss(blob);
    std::string line;
    auto nl = [&]() { std::getline(ss, line); return line; };

    nl(); // CUSTOMER_BEGIN
    Customer c;
    c.id_         = nl();
    c.first_name_ = nl();
    c.last_name_  = nl();
    c.email_      = nl();
    c.phone_      = nl();
    c.address_    = nl();
    int64_t ms    = std::stoll(nl());
    c.created_at_ = std::chrono::system_clock::time_point(
                        std::chrono::milliseconds(ms));

    std::string accts_line = nl(); // "ACCOUNTS:N"
    size_t n = std::stoul(accts_line.substr(9));

    for (size_t i = 0; i < n; ++i) {
        // Read until ACCOUNT_END
        std::ostringstream ab;
        while (std::getline(ss, line)) {
            ab << line << "\n";
            if (line == "ACCOUNT_END") break;
        }
        auto acct_obj = Account::deserialize(ab.str());
        auto acct = std::make_shared<Account>(std::move(acct_obj));
        c.accounts_.push_back(acct);
    }
    // CUSTOMER_END consumed externally
    return c;
}

} // namespace IBS
