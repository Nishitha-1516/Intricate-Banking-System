#pragma once
#include "types.h"
#include "account.h"
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>

namespace IBS {

// ============================================================
//  Customer — a bank client who owns one or more accounts
// ============================================================
class Customer {
public:
    Customer() = default;
    Customer(const CustomerID& id,
             const std::string& first_name,
             const std::string& last_name,
             const std::string& email,
             const std::string& phone,
             const std::string& address = "");

    // ---- Identity ----
    const CustomerID&  id()         const { return id_; }
    const std::string& first_name() const { return first_name_; }
    const std::string& last_name()  const { return last_name_; }
    std::string        full_name()  const { return first_name_ + " " + last_name_; }
    const std::string& email()      const { return email_; }
    const std::string& phone()      const { return phone_; }
    const std::string& address()    const { return address_; }
    Timestamp          created_at() const { return created_at_; }

    // ---- Account management ----
    void add_account(std::shared_ptr<Account> acc);
    bool remove_account(const AccountID& acc_id);
    std::shared_ptr<Account> get_account(const AccountID& acc_id) const;
    const std::vector<std::shared_ptr<Account>>& accounts() const { return accounts_; }
    size_t account_count() const { return accounts_.size(); }

    // ---- Net worth / reporting ----
    Money total_balance() const;
    std::string summary() const;

    // ---- Serialization ----
    std::string serialize() const;
    static Customer deserialize(const std::string& blob);

private:
    CustomerID   id_;
    std::string  first_name_;
    std::string  last_name_;
    std::string  email_;
    std::string  phone_;
    std::string  address_;
    Timestamp    created_at_;
    std::vector<std::shared_ptr<Account>> accounts_;
};

} // namespace IBS
