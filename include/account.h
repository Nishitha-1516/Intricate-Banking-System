#pragma once
#include "types.h"
#include "transaction.h"
#include <vector>
#include <string>
#include <mutex>
#include <stdexcept>

namespace IBS {

// ============================================================
//  Account — holds balance, history, and security state
// ============================================================
class Account {
public:
    Account() = default;
    Account(const AccountID&  id,
            const CustomerID& owner_id,
            AccountType       type,
            Money             initial_balance = Money(0.0));

    // mutex is not movable/copyable — provide explicit move constructor
    Account(Account&& o) noexcept
        : id_(std::move(o.id_)), owner_id_(std::move(o.owner_id_)),
          type_(o.type_), status_(o.status_), balance_(o.balance_),
          history_(std::move(o.history_)), pin_hash_(std::move(o.pin_hash_)),
          failed_logins_(o.failed_logins_), interest_rate_(o.interest_rate_) {}

    Account& operator=(Account&& o) noexcept {
        if (this != &o) {
            id_ = std::move(o.id_); owner_id_ = std::move(o.owner_id_);
            type_ = o.type_; status_ = o.status_; balance_ = o.balance_;
            history_ = std::move(o.history_); pin_hash_ = std::move(o.pin_hash_);
            failed_logins_ = o.failed_logins_; interest_rate_ = o.interest_rate_;
        }
        return *this;
    }
    Account(const Account&)            = delete;
    Account& operator=(const Account&) = delete;

    // ---- Identity ----
    const AccountID&   id()        const { return id_; }
    const CustomerID&  owner_id()  const { return owner_id_; }
    AccountType        type()      const { return type_; }
    AccountStatus      status()    const { return status_; }
    std::string        type_str()  const;
    std::string        status_str()const;

    // ---- Balance ----
    Money balance() const;

    // ---- Transactions ----
    bool deposit   (Money amount, const std::string& note = "");
    bool withdraw  (Money amount, const std::string& note = "");
    bool transfer_to(Account& dest, Money amount, const std::string& note = "");

    const std::vector<Transaction>& history() const { return history_; }
    std::vector<Transaction> recent_history(size_t n) const;

    // ---- Security ----
    bool verify_pin(const std::string& raw_pin) const;
    void set_pin(const std::string& raw_pin);
    void record_failed_login();
    void reset_failed_logins() { failed_logins_ = 0; }
    int  failed_logins() const { return failed_logins_; }
    bool is_locked()  const { return status_ == AccountStatus::LOCKED; }
    void lock()             { status_ = AccountStatus::LOCKED; }
    void unlock()           { status_ = AccountStatus::ACTIVE; }
    void close()            { status_ = AccountStatus::CLOSED; }

    // ---- Interest ----
    double interest_rate() const { return interest_rate_; }
    void set_interest_rate(double r) { interest_rate_ = r; }
    bool apply_interest();

    // ---- Serialization ----
    std::string serialize()  const;
    static Account deserialize(const std::string& blob);

    // ---- Concurrent access ----
    std::mutex& mtx() { return mtx_; }

private:
    AccountID               id_;
    CustomerID              owner_id_;
    AccountType             type_       = AccountType::SAVINGS;
    AccountStatus           status_     = AccountStatus::ACTIVE;
    Money                   balance_;
    std::vector<Transaction> history_;
    std::string             pin_hash_;   // SHA-256-like obfuscation
    int                     failed_logins_ = 0;
    double                  interest_rate_ = 0.035; // 3.5% default
    mutable std::mutex      mtx_;

    static std::string hash_pin(const std::string& raw_pin);
    void push_transaction(Transaction&& t);
};

} // namespace IBS
