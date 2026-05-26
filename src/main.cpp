#include "bank.h"
#include "admin_cli.h"
#include "benchmarker.h"
#include <iostream>
#include <string>
#include <cstring>

// ============================================================
//  Intricate Banking System — main.cpp
//  Usage:
//    ./ibs                  → interactive CLI
//    ./ibs --demo           → run automated demo
//    ./ibs --bench N        → run benchmark with N elements
// ============================================================

void run_demo(IBS::Bank& bank);
void run_benchmark(size_t n);

int main(int argc, char* argv[]) {
    // ── Parse flags ──────────────────────────────────────────
    bool demo_mode  = false;
    bool bench_mode = false;
    size_t bench_n  = 100000;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--demo")  == 0) demo_mode  = true;
        if (std::strcmp(argv[i], "--bench") == 0) {
            bench_mode = true;
            if (i + 1 < argc) bench_n = (size_t)std::stoul(argv[i+1]);
        }
    }

    // ── Benchmark-only mode ───────────────────────────────────
    if (bench_mode && !demo_mode) {
        run_benchmark(bench_n);
        return 0;
    }

    // ── Create bank ───────────────────────────────────────────
    IBS::Bank bank("GlobalTrust Bank of India", "data");
    IBS::AuditLogger::instance().set_echo(false); // quiet for demo

    // ── Try to load previous state ────────────────────────────
    if (bank.load()) {
        std::cout << "✓ Previous database loaded.\n";
    }

    if (demo_mode) {
        run_demo(bank);
        return 0;
    }

    // ── Interactive CLI ───────────────────────────────────────
    IBS::AdminCLI cli(bank);
    cli.run();
    bank.save();
    return 0;
}

// ============================================================
//  Automated demo — seeds data and shows all features
// ============================================================
void run_demo(IBS::Bank& bank) {
    using namespace IBS;

    std::cout << "\n╔══════════════════════════════════════════════╗\n";
    std::cout << "║         AUTOMATED DEMO MODE                  ║\n";
    std::cout << "╚══════════════════════════════════════════════╝\n\n";

    // --- 1. Create customers ---
    std::cout << "► Creating customers...\n";
    auto cid1 = bank.create_customer("Arjun",   "Sharma",  "arjun@example.com",  "+91-9876543210", "Mumbai");
    auto cid2 = bank.create_customer("Priya",   "Nair",    "priya@example.com",  "+91-9012345678", "Bangalore");
    auto cid3 = bank.create_customer("Rahul",   "Gupta",   "rahul@example.com",  "+91-8012345678", "Delhi");
    auto cid4 = bank.create_customer("Meera",   "Patel",   "meera@example.com",  "+91-7012345678", "Hyderabad");
    auto cid5 = bank.create_customer("Vikram",  "Singh",   "vikram@example.com", "+91-6012345678", "Chennai");
    std::cout << "  Created 5 customers.\n";

    // --- 2. Create accounts ---
    std::cout << "\n► Creating accounts...\n";
    auto aid1 = bank.create_account(cid1, AccountType::SAVINGS,       Money(50000.0),  "1234");
    auto aid2 = bank.create_account(cid1, AccountType::CHECKING,      Money(10000.0),  "1234");
    auto aid3 = bank.create_account(cid2, AccountType::SAVINGS,       Money(75000.0),  "5678");
    auto aid4 = bank.create_account(cid3, AccountType::FIXED_DEPOSIT, Money(200000.0), "9999");
    auto aid5 = bank.create_account(cid4, AccountType::SAVINGS,       Money(30000.0),  "4321");
    auto aid6 = bank.create_account(cid5, AccountType::CHECKING,      Money(15000.0),  "1111");
    std::cout << "  Created 6 accounts.\n";

    // --- 3. Deposits ---
    std::cout << "\n► Processing deposits...\n";
    bank.deposit(aid1, Money(25000.0), "Salary credit");
    bank.deposit(aid3, Money(10000.0), "Freelance income");
    bank.deposit(aid5, Money(5000.0),  "Gift");
    std::cout << "  3 deposits completed.\n";

    // --- 4. Withdrawals ---
    std::cout << "\n► Processing withdrawals...\n";
    try {
        bank.withdraw(aid1, Money(15000.0), "1234", "Rent payment");
        bank.withdraw(aid3, Money(20000.0), "5678", "Medical expenses");
        std::cout << "  2 withdrawals completed.\n";
    } catch (const std::exception& e) {
        std::cout << "  Withdrawal error: " << e.what() << "\n";
    }

    // --- 5. Transfers ---
    std::cout << "\n► Processing transfers...\n";
    try {
        bank.transfer(aid1, aid3, Money(5000.0), "1234", "Loan repayment");
        bank.transfer(aid3, aid5, Money(2500.0), "5678", "Shared expense");
        std::cout << "  2 transfers completed.\n";
    } catch (const std::exception& e) {
        std::cout << "  Transfer error: " << e.what() << "\n";
    }

    // --- 6. Undo test ---
    std::cout << "\n► Testing undo...\n";
    bank.withdraw(aid5, Money(1000.0), "4321", "Test withdrawal");
    bool undone = bank.undo_last_transaction(aid5);
    std::cout << "  Undo result: " << (undone ? "SUCCESS" : "FAILED") << "\n";

    // --- 7. Interest ---
    std::cout << "\n► Applying monthly interest...\n";
    int ic = bank.apply_interest_to_all_savings();
    std::cout << "  Applied to " << ic << " accounts.\n";

    // --- 8. Full report ---
    bank.generate_full_report(std::cout);

    // --- 9. Benchmark ---
    std::cout << "\n► Running hash table benchmark (50,000 elements)...\n";
    Benchmarker::compare(50000);
    Benchmarker::print_memory_analysis();

    // --- 10. Save ---
    std::cout << "\n► Saving database...\n";
    bank.save();
    std::cout << "  Database saved to data/bank.dat\n";

    std::cout << "\n╔══════════════════════════════════════════════╗\n";
    std::cout << "║   Demo complete! All features verified.      ║\n";
    std::cout << "╚══════════════════════════════════════════════╝\n";
}

void run_benchmark(size_t n) {
    std::cout << "\nRunning standalone benchmark with n=" << n << "\n";
    IBS::Benchmarker::compare(n);
    IBS::Benchmarker::print_memory_analysis();
}
