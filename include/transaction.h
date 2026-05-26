#pragma once
#include "types.h"
#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <atomic>

namespace IBS {

// ============================================================
//  Transaction — immutable record of a single banking event
// ============================================================
class Transaction {
public:
    // Factory method — generates unique ID automatically
    static Transaction create(TransactionType   type,
                              const AccountID&  from_account,
                              const AccountID&  to_account,
                              Money             amount,
                              const std::string& note = "");

    // Getters
    const TransactionID&   id()          const { return id_; }
    TransactionType        type()        const { return type_; }
    const AccountID&       from()        const { return from_account_; }
    const AccountID&       to()          const { return to_account_; }
    Money                  amount()      const { return amount_; }
    TransactionStatus      status()      const { return status_; }
    Timestamp              timestamp()   const { return timestamp_; }
    const std::string&     note()        const { return note_; }

    // State mutators (used by engine)
    void mark_completed()   { status_ = TransactionStatus::COMPLETED; }
    void mark_failed()      { status_ = TransactionStatus::FAILED; }
    void mark_rolled_back() { status_ = TransactionStatus::ROLLED_BACK; }

    std::string type_str()   const;
    std::string status_str() const;
    std::string timestamp_str() const;
    std::string to_display_string() const;

    // Serialization
    std::string serialize()   const;
    static Transaction deserialize(const std::string& line);

private:
    Transaction() = default;

    TransactionID     id_;
    TransactionType   type_     = TransactionType::DEPOSIT;
    AccountID         from_account_;
    AccountID         to_account_;
    Money             amount_;
    TransactionStatus status_   = TransactionStatus::PENDING;
    Timestamp         timestamp_;
    std::string       note_;

    static std::atomic<uint64_t> counter_;
};

} // namespace IBS
