#pragma once
#include "types.h"
#include "transaction.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <deque>
#include <chrono>
#include <mutex>
#include <iostream>

namespace IBS {

// ============================================================
//  FraudDetector — rule-based real-time fraud detection
//  Rules:
//   1. Large single transaction (> threshold)
//   2. High velocity: > N transactions in last M minutes
//   3. Multiple failed logins → flag account
//   4. Unusual hours (2am–5am large transfers)
//   5. Rapid successive withdrawals from same account
// ============================================================
struct FraudAlert {
    std::string rule_name;
    std::string account_id;
    std::string detail;
    Timestamp   detected_at;
    bool        block_transaction = false;

    std::string to_string() const;
};

class FraudDetector {
public:
    struct Config {
        Money    large_tx_threshold     = Money(50000.0);  // $50k
        int      velocity_max_tx        = 10;              // max 10 tx
        int      velocity_window_mins   = 5;               // in 5 min
        int      unusual_hour_start     = 2;               // 2:00 AM
        int      unusual_hour_end       = 5;               // 5:00 AM
        Money    unusual_hour_threshold = Money(10000.0);  // $10k
        int      rapid_wd_count         = 5;               // 5 withdrawals
        int      rapid_wd_window_secs   = 60;              // in 60s
    };

    FraudDetector();
    explicit FraudDetector(Config cfg);

    // Returns list of triggered alerts; empty = clean
    std::vector<FraudAlert> evaluate(const Transaction& tx,
                                     int failed_logins = 0);

    // Record the tx in velocity window
    void record_transaction(const Transaction& tx);

    // Stats
    size_t total_alerts()  const { return total_alerts_; }
    size_t blocked_count() const { return blocked_count_; }

    void print_report(std::ostream& out = std::cout) const;

private:
    Config cfg_;
    mutable std::mutex mtx_;

    // Per-account deque of recent tx timestamps
    std::unordered_map<std::string, std::deque<Timestamp>> velocity_map_;
    std::unordered_map<std::string, std::deque<Timestamp>> rapid_wd_map_;

    size_t total_alerts_  = 0;
    size_t blocked_count_ = 0;

    std::vector<FraudAlert> rule_large_transaction   (const Transaction& tx);
    std::vector<FraudAlert> rule_velocity            (const Transaction& tx);
    std::vector<FraudAlert> rule_unusual_hours       (const Transaction& tx);
    std::vector<FraudAlert> rule_rapid_withdrawals   (const Transaction& tx);
    std::vector<FraudAlert> rule_failed_logins       (const Transaction& tx, int fails);

    static int hour_of_day(Timestamp t);
    void prune_old(std::deque<Timestamp>& dq, std::chrono::seconds window);
};

} // namespace IBS
