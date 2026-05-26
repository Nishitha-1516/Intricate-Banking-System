#include "transaction.h"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <vector>

namespace IBS {

std::atomic<uint64_t> Transaction::counter_{1};

Transaction Transaction::create(TransactionType   type,
                                const AccountID&  from_account,
                                const AccountID&  to_account,
                                Money             amount,
                                const std::string& note) {
    Transaction t;
    uint64_t seq = counter_.fetch_add(1, std::memory_order_relaxed);
    auto now = std::chrono::system_clock::now();
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                   now.time_since_epoch()).count();

    std::ostringstream ss;
    ss << "TXN" << std::setfill('0') << std::setw(6) << seq
       << "-" << (ms % 1000000);
    t.id_           = ss.str();
    t.type_         = type;
    t.from_account_ = from_account;
    t.to_account_   = to_account;
    t.amount_       = amount;
    t.timestamp_    = now;
    t.note_         = note;
    t.status_       = TransactionStatus::PENDING;
    return t;
}

std::string Transaction::type_str() const {
    switch (type_) {
        case TransactionType::DEPOSIT:    return "DEPOSIT";
        case TransactionType::WITHDRAWAL: return "WITHDRAWAL";
        case TransactionType::TRANSFER:   return "TRANSFER";
        case TransactionType::INTEREST:   return "INTEREST";
        case TransactionType::FEE:        return "FEE";
    }
    return "UNKNOWN";
}

std::string Transaction::status_str() const {
    switch (status_) {
        case TransactionStatus::PENDING:      return "PENDING";
        case TransactionStatus::COMPLETED:    return "COMPLETED";
        case TransactionStatus::FAILED:       return "FAILED";
        case TransactionStatus::ROLLED_BACK:  return "ROLLED_BACK";
    }
    return "UNKNOWN";
}

std::string Transaction::timestamp_str() const {
    auto t = std::chrono::system_clock::to_time_t(timestamp_);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string Transaction::to_display_string() const {
    std::ostringstream ss;
    ss << "  [" << id_ << "] "
       << std::left << std::setw(11) << type_str()
       << " " << amount_.to_string()
       << " | " << timestamp_str()
       << " | " << std::setw(12) << status_str();
    if (!note_.empty()) ss << " | " << note_;
    return ss.str();
}

// ---- Serialization: pipe-delimited single line ----
std::string Transaction::serialize() const {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  timestamp_.time_since_epoch()).count();
    std::ostringstream ss;
    ss << id_ << "|"
       << static_cast<int>(type_) << "|"
       << from_account_ << "|"
       << to_account_ << "|"
       << amount_.cents << "|"
       << static_cast<int>(status_) << "|"
       << ms << "|"
       << note_;
    return ss.str();
}

Transaction Transaction::deserialize(const std::string& line) {
    std::istringstream ss(line);
    std::string tok;
    std::vector<std::string> fields;
    while (std::getline(ss, tok, '|')) fields.push_back(tok);
    if (fields.size() < 8)
        throw std::runtime_error("Transaction::deserialize: malformed line");

    Transaction t;
    t.id_           = fields[0];
    t.type_         = static_cast<TransactionType>(std::stoi(fields[1]));
    t.from_account_ = fields[2];
    t.to_account_   = fields[3];
    t.amount_ = Money(static_cast<int64_t>(std::stoll(fields[4])));
    t.status_       = static_cast<TransactionStatus>(std::stoi(fields[5]));
    int64_t ms      = std::stoll(fields[6]);
    t.timestamp_    = std::chrono::system_clock::time_point(
                          std::chrono::milliseconds(ms));
    t.note_         = fields[7];
    return t;
}

} // namespace IBS
