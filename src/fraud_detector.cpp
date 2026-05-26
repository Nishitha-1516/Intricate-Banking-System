#include "fraud_detector.h"
#include <sstream>
#include <iomanip>
#include <iostream>
#include <ctime>
#include <algorithm>

namespace IBS {

FraudDetector::FraudDetector() : cfg_(Config{}) {}
FraudDetector::FraudDetector(Config cfg) : cfg_(cfg) {}

std::string FraudAlert::to_string() const {
    std::ostringstream ss;
    auto t = std::chrono::system_clock::to_time_t(detected_at);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    ss << "  ⚠ FRAUD[" << rule_name << "] acct=" << account_id
       << " | " << detail
       << (block_transaction ? " [BLOCKED]" : " [FLAGGED]")
       << " @ " << std::put_time(&tm_buf, "%H:%M:%S");
    return ss.str();
}

int FraudDetector::hour_of_day(Timestamp t) {
    auto tt = std::chrono::system_clock::to_time_t(t);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &tt);
#else
    localtime_r(&tt, &tm_buf);
#endif
    return tm_buf.tm_hour;
}

void FraudDetector::prune_old(std::deque<Timestamp>& dq, std::chrono::seconds window) {
    auto cutoff = std::chrono::system_clock::now() - window;
    while (!dq.empty() && dq.front() < cutoff)
        dq.pop_front();
}

std::vector<FraudAlert> FraudDetector::rule_large_transaction(const Transaction& tx) {
    std::vector<FraudAlert> alerts;
    if (tx.amount() >= cfg_.large_tx_threshold) {
        FraudAlert a;
        a.rule_name  = "LARGE_TX";
        a.account_id = tx.from().empty() ? tx.to() : tx.from();
        a.detail     = "Amount " + tx.amount().to_string() +
                       " exceeds threshold " + cfg_.large_tx_threshold.to_string();
        a.detected_at = std::chrono::system_clock::now();
        a.block_transaction = false; // flag only
        alerts.push_back(a);
    }
    return alerts;
}

std::vector<FraudAlert> FraudDetector::rule_velocity(const Transaction& tx) {
    std::vector<FraudAlert> alerts;
    const auto& aid = tx.from().empty() ? tx.to() : tx.from();
    auto& dq = velocity_map_[aid];
    prune_old(dq, std::chrono::seconds(cfg_.velocity_window_mins * 60));
    if ((int)dq.size() >= cfg_.velocity_max_tx) {
        FraudAlert a;
        a.rule_name = "HIGH_VELOCITY";
        a.account_id = aid;
        a.detail = std::to_string(dq.size()) + " transactions in " +
                   std::to_string(cfg_.velocity_window_mins) + " minutes";
        a.detected_at = std::chrono::system_clock::now();
        a.block_transaction = true;
        alerts.push_back(a);
    }
    return alerts;
}

std::vector<FraudAlert> FraudDetector::rule_unusual_hours(const Transaction& tx) {
    std::vector<FraudAlert> alerts;
    if (tx.type() != TransactionType::TRANSFER && tx.type() != TransactionType::WITHDRAWAL)
        return alerts;
    int h = hour_of_day(tx.timestamp());
    if (h >= cfg_.unusual_hour_start && h < cfg_.unusual_hour_end &&
        tx.amount() >= cfg_.unusual_hour_threshold) {
        FraudAlert a;
        a.rule_name = "UNUSUAL_HOURS";
        a.account_id = tx.from().empty() ? tx.to() : tx.from();
        a.detail = tx.amount().to_string() + " at " + std::to_string(h) + ":00";
        a.detected_at = std::chrono::system_clock::now();
        a.block_transaction = false;
        alerts.push_back(a);
    }
    return alerts;
}

std::vector<FraudAlert> FraudDetector::rule_rapid_withdrawals(const Transaction& tx) {
    std::vector<FraudAlert> alerts;
    if (tx.type() != TransactionType::WITHDRAWAL) return alerts;
    const auto& aid = tx.from();
    if (aid.empty()) return alerts;
    auto& dq = rapid_wd_map_[aid];
    prune_old(dq, std::chrono::seconds(cfg_.rapid_wd_window_secs));
    if ((int)dq.size() >= cfg_.rapid_wd_count) {
        FraudAlert a;
        a.rule_name = "RAPID_WITHDRAWALS";
        a.account_id = aid;
        a.detail = std::to_string(dq.size()) + " withdrawals in " +
                   std::to_string(cfg_.rapid_wd_window_secs) + "s";
        a.detected_at = std::chrono::system_clock::now();
        a.block_transaction = true;
        alerts.push_back(a);
    }
    return alerts;
}

std::vector<FraudAlert> FraudDetector::rule_failed_logins(const Transaction& tx, int fails) {
    std::vector<FraudAlert> alerts;
    if (fails >= 2) {
        FraudAlert a;
        a.rule_name = "FAILED_LOGINS";
        a.account_id = tx.from().empty() ? tx.to() : tx.from();
        a.detail = std::to_string(fails) + " failed login attempts";
        a.detected_at = std::chrono::system_clock::now();
        a.block_transaction = (fails >= 3);
        alerts.push_back(a);
    }
    return alerts;
}

std::vector<FraudAlert> FraudDetector::evaluate(const Transaction& tx, int failed_logins) {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<FraudAlert> all;
    auto merge = [&](auto v) { for (auto& a : v) all.push_back(std::move(a)); };
    merge(rule_large_transaction(tx));
    merge(rule_velocity(tx));
    merge(rule_unusual_hours(tx));
    merge(rule_rapid_withdrawals(tx));
    merge(rule_failed_logins(tx, failed_logins));

    total_alerts_ += all.size();
    for (const auto& a : all) if (a.block_transaction) ++blocked_count_;
    return all;
}

void FraudDetector::record_transaction(const Transaction& tx) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto now = std::chrono::system_clock::now();
    const auto& aid = tx.from().empty() ? tx.to() : tx.from();
    velocity_map_[aid].push_back(now);
    if (tx.type() == TransactionType::WITHDRAWAL)
        rapid_wd_map_[aid].push_back(now);
}

void FraudDetector::print_report(std::ostream& out) const {
    out << "\n╔══════════════════════════════════╗\n";
    out << "║     Fraud Detection Report       ║\n";
    out << "╠══════════════════════════════════╣\n";
    out << "║  Total alerts  : " << std::setw(14) << total_alerts_  << " ║\n";
    out << "║  Blocked txns  : " << std::setw(14) << blocked_count_ << " ║\n";
    out << "║  Monitored accts: " << std::setw(13) << velocity_map_.size() << " ║\n";
    out << "╚══════════════════════════════════╝\n";
}

} // namespace IBS
