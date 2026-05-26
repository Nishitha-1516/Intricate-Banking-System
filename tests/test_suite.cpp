// ============================================================
//  Intricate Banking System — Test Suite
//  Run: ./ibs_tests
// ============================================================
#include "bank.h"
#include "hash_table.h"
#include "lru_cache.h"
#include "benchmarker.h"
#include <iostream>
#include <cassert>
#include <stdexcept>
#include <sstream>
#include <filesystem>

using namespace IBS;

// ---- Minimal test harness ----
static int tests_run = 0, tests_passed = 0;

#define TEST(name) \
    ++tests_run; \
    try { \
        std::cout << "  [TEST] " << #name << " ... "; \
        test_##name(); \
        ++tests_passed; \
        std::cout << "PASS\n"; \
    } catch (const std::exception& e) { \
        std::cout << "FAIL: " << e.what() << "\n"; \
    } catch (...) { \
        std::cout << "FAIL: unknown exception\n"; \
    }

#define ASSERT(cond)            do { if (!(cond)) throw std::runtime_error("Assert failed: " #cond); } while(0)
#define ASSERT_EQ(a,b)          do { if ((a)!=(b)) throw std::runtime_error(std::string("Assert EQ failed: ") + std::to_string(a) + " != " + std::to_string(b)); } while(0)
#define ASSERT_THROWS(expr)     do { bool threw=false; try { expr; } catch(...){ threw=true; } if(!threw) throw std::runtime_error("Expected exception not thrown"); } while(0)

// ============================================================
//  Money tests
// ============================================================
void test_money_arithmetic() {
    Money a(100.0), b(50.0);
    ASSERT((a + b).cents == 15000);
    ASSERT((a - b).cents == 5000);
    ASSERT(a >= b);
    ASSERT(b < a);
    ASSERT(Money(1.01).cents == 101);
    ASSERT(Money(-10.0).cents == -1000);
}

void test_money_string() {
    ASSERT(Money(100.0).to_string()  == "$100.00");
    ASSERT(Money(0.50).to_string()   == "$0.50");
    ASSERT(Money(1000.99).to_string()== "$1000.99");
}

// ============================================================
//  Hash table tests
// ============================================================
void test_hash_insert_find() {
    DoubleHashTable<std::string, int> ht(17);
    ASSERT(ht.insert("hello", 1));
    ASSERT(ht.insert("world", 2));
    ASSERT(ht.find("hello").value() == 1);
    ASSERT(ht.find("world").value() == 2);
    ASSERT(!ht.find("missing").has_value());
}

void test_hash_update() {
    DoubleHashTable<std::string, int> ht(17);
    ht.insert("key", 10);
    ht.insert("key", 99);  // overwrite
    ASSERT_EQ(ht.find("key").value(), 99);
    ASSERT_EQ(ht.size(), (size_t)1);
}

void test_hash_remove() {
    DoubleHashTable<std::string, int> ht(17);
    ht.insert("a", 1);
    ht.insert("b", 2);
    ht.remove("a");
    ASSERT(!ht.find("a").has_value());
    ASSERT(ht.find("b").value() == 2);
    ASSERT_EQ(ht.size(), (size_t)1);
}

void test_hash_resize() {
    // Fill past load threshold to trigger resize
    DoubleHashTable<std::string, int> ht(7);
    for (int i = 0; i < 30; ++i)
        ht.insert("key_" + std::to_string(i), i);
    // Verify all still accessible
    for (int i = 0; i < 30; ++i)
        ASSERT(ht.find("key_" + std::to_string(i)).value() == i);
    ASSERT(ht.stats().resize_events > 0);
}

void test_hash_collision_rate_low() {
    DoubleHashTable<std::string, int> ht(1 << 14);
    auto ids = Benchmarker::gen_random_ids(1000);
    for (size_t i = 0; i < ids.size(); ++i) ht.insert(ids[i], (int)i);
    // Collision rate should be under 15%
    ASSERT(ht.stats().insert_collision_rate() < 0.15);
}

// ============================================================
//  LRU Cache tests
// ============================================================
void test_lru_basic() {
    LRUCache<std::string, int> cache(3);
    cache.put("a", 1);
    cache.put("b", 2);
    cache.put("c", 3);
    ASSERT(cache.get("a").value() == 1);
    cache.put("d", 4); // evicts b (LRU)
    ASSERT(!cache.get("b").has_value());
    ASSERT(cache.get("d").value() == 4);
}

void test_lru_eviction_order() {
    LRUCache<int, int> cache(2);
    cache.put(1, 10);
    cache.put(2, 20);
    cache.get(1);      // 1 is now MRU
    cache.put(3, 30);  // evicts 2
    ASSERT(!cache.get(2).has_value());
    ASSERT(cache.get(1).value() == 10);
}

void test_lru_update() {
    LRUCache<std::string, int> cache(3);
    cache.put("x", 5);
    cache.put("x", 99);
    ASSERT_EQ(cache.get("x").value(), 99);
    ASSERT_EQ(cache.size(), (size_t)1);
}

// ============================================================
//  Transaction tests
// ============================================================
void test_transaction_create() {
    auto t = Transaction::create(TransactionType::DEPOSIT, "", "ACC001",
                                  Money(500.0), "Test");
    ASSERT(!t.id().empty());
    ASSERT(t.type() == TransactionType::DEPOSIT);
    ASSERT(t.amount().cents == 50000);
    ASSERT(t.status() == TransactionStatus::PENDING);
}

void test_transaction_serialize() {
    auto t = Transaction::create(TransactionType::TRANSFER, "A1", "A2",
                                  Money(1234.56), "Test note");
    t.mark_completed();
    std::string s = t.serialize();
    auto t2 = Transaction::deserialize(s);
    ASSERT(t2.id() == t.id());
    ASSERT(t2.amount().cents == t.amount().cents);
    ASSERT(t2.status() == TransactionStatus::COMPLETED);
    ASSERT(t2.note() == "Test note");
}

// ============================================================
//  Account tests
// ============================================================
void test_account_deposit() {
    Account a("ACC001", "CUST001", AccountType::SAVINGS, Money(1000.0));
    a.set_pin("1234");
    ASSERT(a.deposit(Money(500.0)));
    ASSERT(a.balance().cents == 150000);
}

void test_account_withdraw_sufficient() {
    Account a("ACC002", "CUST001", AccountType::CHECKING, Money(1000.0));
    a.set_pin("1234");
    ASSERT(a.withdraw(Money(400.0)));
    ASSERT(a.balance().cents == 60000);
}

void test_account_withdraw_insufficient() {
    Account a("ACC003", "CUST001", AccountType::SAVINGS, Money(100.0));
    ASSERT_THROWS(a.withdraw(Money(500.0)));
}

void test_account_pin_lock() {
    Account a("ACC004", "CUST001", AccountType::SAVINGS, Money(100.0));
    a.set_pin("9999");
    ASSERT(!a.verify_pin("0000"));
    a.record_failed_login();
    a.record_failed_login();
    a.record_failed_login();  // 3rd → locked
    ASSERT(a.is_locked());
}

void test_account_interest() {
    Account a("ACC005", "CUST001", AccountType::SAVINGS, Money(12000.0));
    a.set_interest_rate(0.12); // 12% annually = 1% monthly
    a.apply_interest();
    // 12000 * 0.12 / 12 = 120 → balance = 12120
    ASSERT(a.balance().cents == 1212000);
}

void test_account_serialize() {
    Account a("ACC006", "CUST001", AccountType::SAVINGS, Money(5000.0));
    a.set_pin("4321");
    a.deposit(Money(1000.0), "Test deposit");
    std::string blob = a.serialize();
    Account b = Account::deserialize(blob);
    ASSERT(b.id()           == "ACC006");
    ASSERT(b.balance().cents == a.balance().cents);
    ASSERT(b.history().size()== a.history().size());
}

// ============================================================
//  Bank integration tests
// ============================================================
void test_bank_create_customer() {
    Bank bank("Test Bank", "data/test_tmp");
    auto cid = bank.create_customer("John", "Doe", "j@test.com", "123", "Addr");
    ASSERT(!cid.empty());
    auto c = bank.find_customer(cid);
    ASSERT(c != nullptr);
    ASSERT(c->full_name() == "John Doe");
}

void test_bank_create_account() {
    Bank bank("Test Bank", "data/test_tmp");
    auto cid = bank.create_customer("Jane", "Doe", "jane@test.com", "456");
    auto aid = bank.create_account(cid, AccountType::SAVINGS, Money(5000.0), "0000");
    ASSERT(!aid.empty());
    auto a = bank.find_account(aid);
    ASSERT(a != nullptr);
    ASSERT(a->balance().cents == 500000);
}

void test_bank_deposit_withdraw() {
    Bank bank("Test Bank", "data/test_tmp");
    auto cid = bank.create_customer("Test", "User", "t@t.com", "000");
    auto aid = bank.create_account(cid, AccountType::SAVINGS, Money(1000.0), "1111");
    bank.deposit(aid, Money(500.0), "top-up");
    bank.withdraw(aid, Money(200.0), "1111", "bills");
    ASSERT(bank.find_account(aid)->balance().cents == 130000);
}

void test_bank_transfer() {
    Bank bank("Test Bank", "data/test_tmp");
    auto c1 = bank.create_customer("A", "A", "a@a.com", "1");
    auto c2 = bank.create_customer("B", "B", "b@b.com", "2");
    auto a1 = bank.create_account(c1, AccountType::SAVINGS,  Money(10000.0), "1234");
    auto a2 = bank.create_account(c2, AccountType::CHECKING, Money(0.0),     "5678");
    bank.transfer(a1, a2, Money(3000.0), "1234", "payment");
    ASSERT(bank.find_account(a1)->balance().cents == 700000);
    ASSERT(bank.find_account(a2)->balance().cents == 300000);
}

void test_bank_undo() {
    Bank bank("Test Bank", "data/test_tmp");
    auto cid = bank.create_customer("U", "U", "u@u.com", "0");
    auto aid = bank.create_account(cid, AccountType::SAVINGS, Money(5000.0), "2222");
    bank.withdraw(aid, Money(1000.0), "2222", "test");
    ASSERT(bank.find_account(aid)->balance().cents == 400000);
    bank.undo_last_transaction(aid);
    ASSERT(bank.find_account(aid)->balance().cents == 500000);
}

void test_bank_interest_engine() {
    Bank bank("Test Bank", "data/test_tmp");
    auto cid = bank.create_customer("I", "I", "i@i.com", "0");
    auto aid = bank.create_account(cid, AccountType::SAVINGS, Money(12000.0), "3333");
    bank.find_account(aid)->set_interest_rate(0.12);
    int n = bank.apply_interest_to_all_savings();
    ASSERT(n >= 1);
    ASSERT(bank.find_account(aid)->balance().cents > 1200000);
}

void test_bank_save_load() {
    std::filesystem::create_directories("data/test_tmp");
    {
        Bank bank("Save Test Bank", "data/test_tmp");
        auto cid = bank.create_customer("S", "L", "sl@sl.com", "0");
        bank.create_account(cid, AccountType::SAVINGS, Money(9999.0), "7777");
        ASSERT(bank.save("data/test_tmp/test_save.dat"));
    }
    {
        Bank bank("Save Test Bank", "data/test_tmp");
        ASSERT(bank.load("data/test_tmp/test_save.dat"));
        ASSERT(bank.customer_count() >= 1);
    }
    std::filesystem::remove("data/test_tmp/test_save.dat");
}

// ============================================================
//  Main
// ============================================================
int main() {
    std::cout << "╔══════════════════════════════════════╗\n";
    std::cout << "║  Intricate Banking System — Tests    ║\n";
    std::cout << "╚══════════════════════════════════════╝\n\n";

    std::cout << "[ Money Tests ]\n";
    TEST(money_arithmetic)
    TEST(money_string)

    std::cout << "\n[ Hash Table Tests ]\n";
    TEST(hash_insert_find)
    TEST(hash_update)
    TEST(hash_remove)
    TEST(hash_resize)
    TEST(hash_collision_rate_low)

    std::cout << "\n[ LRU Cache Tests ]\n";
    TEST(lru_basic)
    TEST(lru_eviction_order)
    TEST(lru_update)

    std::cout << "\n[ Transaction Tests ]\n";
    TEST(transaction_create)
    TEST(transaction_serialize)

    std::cout << "\n[ Account Tests ]\n";
    TEST(account_deposit)
    TEST(account_withdraw_sufficient)
    TEST(account_withdraw_insufficient)
    TEST(account_pin_lock)
    TEST(account_interest)
    TEST(account_serialize)

    std::cout << "\n[ Bank Integration Tests ]\n";
    TEST(bank_create_customer)
    TEST(bank_create_account)
    TEST(bank_deposit_withdraw)
    TEST(bank_transfer)
    TEST(bank_undo)
    TEST(bank_interest_engine)
    TEST(bank_save_load)

    std::cout << "\n══════════════════════════════════════\n";
    std::cout << "  Results: " << tests_passed << "/" << tests_run << " passed";
    if (tests_passed == tests_run)
        std::cout << "  ✓ ALL TESTS PASSED\n";
    else
        std::cout << "  ✗ " << (tests_run - tests_passed) << " FAILED\n";
    std::cout << "══════════════════════════════════════\n";

    return (tests_passed == tests_run) ? 0 : 1;
}
