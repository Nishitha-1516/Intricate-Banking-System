#pragma once
#include "types.h"
#include "customer.h"
#include "account.h"
#include "transaction.h"
#include "hash_table.h"
#include "lru_cache.h"
#include "audit_logger.h"
#include "fraud_detector.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <stack>
#include <fstream>

namespace IBS {

// ============================================================
//  TransactionRequest — submitted to the async engine
// ============================================================
struct TransactionRequest {
    TransactionType  type;
    AccountID        from;
    AccountID        to;
    Money            amount;
    std::string      note;
    std::function<void(bool, const std::string&)> callback;
};

// ============================================================
//  Bank — central orchestrator
//  Features:
//   - Customer/Account CRUD with double-hash indexing
//   - Multithreaded transaction processing (thread pool)
//   - LRU cache for hot account lookups
//   - Undo/redo stack for failed transactions
//   - Interest calculation engine
//   - Fraud detection integration
//   - Persistent binary serialization
// ============================================================
class Bank {
public:
    explicit Bank(const std::string& name,
                  const std::string& data_dir = "data");
    ~Bank();

    // ---- Customer Management ----
    CustomerID create_customer(const std::string& first, const std::string& last,
                               const std::string& email, const std::string& phone,
                               const std::string& address = "");
    bool delete_customer(const CustomerID& cid);
    std::shared_ptr<Customer> find_customer(const CustomerID& cid) const;
    std::shared_ptr<Customer> search_customer_by_email(const std::string& email) const;
    std::vector<std::shared_ptr<Customer>> all_customers() const;

    // ---- Account Management ----
    AccountID create_account(const CustomerID& cid, AccountType type,
                             Money initial_deposit, const std::string& raw_pin);
    bool close_account(const AccountID& aid);
    std::shared_ptr<Account> find_account(const AccountID& aid);

    // ---- Transactions (synchronous) ----
    bool deposit   (const AccountID& aid, Money amount, const std::string& note = "");
    bool withdraw  (const AccountID& aid, Money amount, const std::string& pin, const std::string& note = "");
    bool transfer  (const AccountID& from, const AccountID& to, Money amount,
                    const std::string& pin, const std::string& note = "");

    // ---- Async transaction submission ----
    void submit_transaction(TransactionRequest req);

    // ---- Undo / Redo ----
    bool undo_last_transaction(const AccountID& aid);

    // ---- Interest Engine ----
    int apply_interest_to_all_savings();

    // ---- Security ----
    bool verify_pin(const AccountID& aid, const std::string& raw_pin);

    // ---- Persistence ----
    bool save(const std::string& filename = "");
    bool load(const std::string& filename = "");

    // ---- Reporting ----
    void print_summary(std::ostream& out = std::cout) const;
    void print_hash_stats(std::ostream& out = std::cout) const;
    void print_cache_stats(std::ostream& out = std::cout) const;
    void print_fraud_report(std::ostream& out = std::cout) const;
    void generate_full_report(std::ostream& out = std::cout) const;

    // ---- Getters ----
    const std::string& name()     const { return name_; }
    size_t customer_count()       const { return customers_.size(); }
    size_t account_count()        const { return account_index_.size(); }
    size_t pending_tx_count()     const { return pending_count_.load(); }

private:
    std::string name_;
    std::string data_dir_;

    // Primary storage
    std::unordered_map<CustomerID, std::shared_ptr<Customer>> customers_;
    // O(1) account lookup index
    DoubleHashTable<AccountID, std::shared_ptr<Account>>      account_index_;
    // LRU hot-path cache
    LRUCache<AccountID, std::shared_ptr<Account>>             account_cache_;
    // Fraud engine
    FraudDetector                                             fraud_;

    // Undo stacks per account (most recent transaction)
    std::unordered_map<AccountID, std::stack<Transaction>>    undo_stacks_;

    // Thread pool for async transactions
    static constexpr int THREAD_POOL_SIZE = 4;
    std::vector<std::thread>          workers_;
    std::queue<TransactionRequest>    tx_queue_;
    std::mutex                        queue_mtx_;
    std::condition_variable           queue_cv_;
    std::atomic<bool>                 stop_workers_{false};
    std::atomic<size_t>               pending_count_{0};

    mutable std::mutex                bank_mtx_;

    // ID generation
    static std::atomic<uint64_t>      cust_counter_;
    static std::atomic<uint64_t>      acct_counter_;

    void worker_loop();
    void process_request(TransactionRequest& req);
    std::shared_ptr<Account> get_account_cached(const AccountID& aid);

    static std::string gen_customer_id();
    static std::string gen_account_id(AccountType t);
};

} // namespace IBS
