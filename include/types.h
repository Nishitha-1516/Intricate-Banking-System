#pragma once
#include <string>
#include <cstdint>
#include <chrono>

// ============================================================
//  Intricate Banking System — Core Types
//  Author: Generated for Portfolio / Resume Use
// ============================================================

namespace IBS {

using AccountID   = std::string;
using CustomerID  = std::string;
using TransactionID = std::string;
using Timestamp   = std::chrono::system_clock::time_point;

enum class AccountType   { SAVINGS, CHECKING, FIXED_DEPOSIT };
enum class TransactionType { DEPOSIT, WITHDRAWAL, TRANSFER, INTEREST, FEE };
enum class TransactionStatus { PENDING, COMPLETED, FAILED, ROLLED_BACK };
enum class AccountStatus { ACTIVE, LOCKED, CLOSED, SUSPENDED };

struct Money {
    int64_t cents = 0;  // store as integer cents to avoid float rounding

    explicit Money(double dollars = 0.0)
        : cents(static_cast<int64_t>(dollars >= 0 ? dollars * 100 + 0.5 : dollars * 100 - 0.5)) {}
    explicit Money(int64_t c) : cents(c) {}

    double to_double() const { return cents / 100.0; }
    bool operator>=(const Money& o) const { return cents >= o.cents; }
    bool operator<(const Money& o)  const { return cents <  o.cents; }
    bool operator<=(const Money& o) const { return cents <= o.cents; }
    Money operator+(const Money& o) const { return Money(cents + o.cents); }
    Money operator-(const Money& o) const { return Money(cents - o.cents); }
    bool operator==(const Money& o) const { return cents == o.cents; }
    bool operator!=(const Money& o) const { return cents != o.cents; }
    std::string to_string() const {
        bool neg = cents < 0;
        int64_t abs_c = neg ? -cents : cents;
        return (neg ? "-$" : "$") +
               std::to_string(abs_c / 100) + "." +
               (abs_c % 100 < 10 ? "0" : "") +
               std::to_string(abs_c % 100);
    }
};

} // namespace IBS
