#pragma once
#include "bank.h"
#include "benchmarker.h"
#include <iostream>
#include <string>
#include <limits>
#include <iomanip>
#include <thread>
#include <chrono>

namespace IBS {

// ============================================================
//  AdminCLI — full menu-driven terminal dashboard
// ============================================================
class AdminCLI {
public:
    explicit AdminCLI(Bank& bank) : bank_(bank) {}

    void run() {
        print_banner();
        bool running = true;
        while (running) {
            print_main_menu();
            int choice = get_int("Choice");
            switch (choice) {
                case 1:  create_account_flow();    break;
                case 2:  delete_account_flow();    break;
                case 3:  search_customer_flow();   break;
                case 4:  transaction_flow();       break;
                case 5:  view_history_flow();      break;
                case 6:  balance_inquiry_flow();   break;
                case 7:  apply_interest_flow();    break;
                case 8:  undo_transaction_flow();  break;
                case 9:  generate_reports_flow();  break;
                case 10: benchmark_flow();         break;
                case 11: save_load_flow();         break;
                case 0:  running = false; std::cout << "\nGoodbye!\n"; break;
                default: std::cout << "Invalid option.\n";
            }
        }
    }

private:
    Bank& bank_;

    // ---- Helpers ----
    static void clear_input() {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    static int get_int(const std::string& prompt) {
        int v;
        std::cout << prompt << ": ";
        while (!(std::cin >> v)) { clear_input(); std::cout << "Enter a number: "; }
        clear_input();
        return v;
    }

    static double get_double(const std::string& prompt) {
        double v;
        std::cout << prompt << ": ";
        while (!(std::cin >> v) || v < 0) {
            clear_input();
            std::cout << "Enter a positive number: ";
        }
        clear_input();
        return v;
    }

    static std::string get_string(const std::string& prompt) {
        std::cout << prompt << ": ";
        std::string s;
        std::getline(std::cin, s);
        return s;
    }

    static void pause() {
        std::cout << "\n[Press ENTER to continue]";
        std::cin.get();
    }

    // ---- UI ----
    static void print_banner() {
        std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║                                                              ║
║      ██╗██████╗ ███████╗                                     ║
║      ██║██╔══██╗██╔════╝                                     ║
║      ██║██████╔╝███████╗                                     ║
║      ██║██╔══██╗╚════██║                                     ║
║      ██║██████╔╝███████║                                     ║
║      ╚═╝╚═════╝ ╚══════╝                                     ║
║                                                              ║
║         Intricate Banking System  v1.0.0                     ║
║         C++17 | Hash Tables | Multithreading | LRU           ║
║                                                              ║
╚══════════════════════════════════════════════════════════════╝
)";
    }

    static void print_main_menu() {
        std::cout << "\n┌─────────────────────────────────────┐\n";
        std::cout << "│          ADMIN DASHBOARD            │\n";
        std::cout << "├─────────────────────────────────────┤\n";
        std::cout << "│  1.  Create Account                 │\n";
        std::cout << "│  2.  Delete Account / Customer      │\n";
        std::cout << "│  3.  Search Customer                │\n";
        std::cout << "│  4.  Deposit / Withdraw / Transfer  │\n";
        std::cout << "│  5.  View Transaction History       │\n";
        std::cout << "│  6.  Balance Inquiry                │\n";
        std::cout << "│  7.  Apply Monthly Interest         │\n";
        std::cout << "│  8.  Undo Last Transaction          │\n";
        std::cout << "│  9.  Generate Full Report           │\n";
        std::cout << "│  10. Hash Table Benchmark           │\n";
        std::cout << "│  11. Save / Load Database           │\n";
        std::cout << "│  0.  Exit                           │\n";
        std::cout << "└─────────────────────────────────────┘\n";
    }

    // ---- Flows ----
    void create_account_flow() {
        std::cout << "\n=== Create New Account ===\n";
        std::string first   = get_string("First name");
        std::string last    = get_string("Last name");
        std::string email   = get_string("Email");
        std::string phone   = get_string("Phone");
        std::string address = get_string("Address (optional)");

        std::cout << "Account type: 1=SAVINGS  2=CHECKING  3=FIXED_DEPOSIT\n";
        int t = get_int("Type");
        AccountType atype = AccountType::SAVINGS;
        if (t == 2) atype = AccountType::CHECKING;
        if (t == 3) atype = AccountType::FIXED_DEPOSIT;

        double init = get_double("Initial deposit ($)");
        std::string pin = get_string("Set PIN (min 4 digits)");

        try {
            auto cid = bank_.create_customer(first, last, email, phone, address);
            auto aid = bank_.create_account(cid, atype, Money(init), pin);
            std::cout << "\n✓ Customer ID : " << cid << "\n";
            std::cout << "✓ Account ID  : " << aid << "\n";
        } catch (const std::exception& e) {
            std::cout << "✗ Error: " << e.what() << "\n";
        }
        pause();
    }

    void delete_account_flow() {
        std::cout << "\n=== Delete Customer ===\n";
        std::string cid = get_string("Customer ID");
        auto c = bank_.find_customer(cid);
        if (!c) { std::cout << "Customer not found.\n"; pause(); return; }
        std::cout << "Are you sure you want to delete " << c->full_name() << "? (y/N): ";
        char ch; std::cin >> ch; clear_input();
        if (ch == 'y' || ch == 'Y') {
            bank_.delete_customer(cid);
            std::cout << "✓ Customer deleted.\n";
        } else {
            std::cout << "Cancelled.\n";
        }
        pause();
    }

    void search_customer_flow() {
        std::cout << "\n=== Search Customer ===\n";
        std::cout << "1. By ID\n2. By Email\n";
        int opt = get_int("Option");
        std::shared_ptr<Customer> c;
        if (opt == 1) {
            c = bank_.find_customer(get_string("Customer ID"));
        } else {
            c = bank_.search_customer_by_email(get_string("Email"));
        }
        if (!c) { std::cout << "Not found.\n"; }
        else     { std::cout << c->summary(); }
        pause();
    }

    void transaction_flow() {
        std::cout << "\n=== Transaction ===\n";
        std::cout << "1. Deposit\n2. Withdraw\n3. Transfer\n";
        int opt = get_int("Type");

        if (opt == 1) {
            std::string aid = get_string("Account ID");
            double amt = get_double("Amount ($)");
            std::string note = get_string("Note");
            try {
                bank_.deposit(aid, Money(amt), note);
                std::cout << "✓ Deposit successful.\n";
            } catch (const std::exception& e) {
                std::cout << "✗ " << e.what() << "\n";
            }
        } else if (opt == 2) {
            std::string aid = get_string("Account ID");
            double amt = get_double("Amount ($)");
            std::string pin = get_string("PIN");
            std::string note = get_string("Note");
            try {
                bank_.withdraw(aid, Money(amt), pin, note);
                std::cout << "✓ Withdrawal successful.\n";
            } catch (const std::exception& e) {
                std::cout << "✗ " << e.what() << "\n";
            }
        } else if (opt == 3) {
            std::string from = get_string("From Account ID");
            std::string to   = get_string("To Account ID");
            double amt = get_double("Amount ($)");
            std::string pin = get_string("PIN");
            std::string note = get_string("Note");
            try {
                bank_.transfer(from, to, Money(amt), pin, note);
                std::cout << "✓ Transfer successful.\n";
            } catch (const std::exception& e) {
                std::cout << "✗ " << e.what() << "\n";
            }
        }
        pause();
    }

    void view_history_flow() {
        std::cout << "\n=== Transaction History ===\n";
        std::string aid = get_string("Account ID");
        auto acct = bank_.find_account(aid);
        if (!acct) { std::cout << "Account not found.\n"; pause(); return; }

        int n = get_int("Show last N transactions (0 = all)");
        const auto& hist = acct->history();
        auto display = (n <= 0 || (size_t)n >= hist.size()) ?
                       hist : std::vector<Transaction>(hist.end()-n, hist.end());

        std::cout << "\n--- History for " << aid << " ---\n";
        for (const auto& t : display)
            std::cout << t.to_display_string() << "\n";
        std::cout << "Total transactions: " << hist.size() << "\n";
        pause();
    }

    void balance_inquiry_flow() {
        std::cout << "\n=== Balance Inquiry ===\n";
        std::string aid = get_string("Account ID");
        auto acct = bank_.find_account(aid);
        if (!acct) { std::cout << "Account not found.\n"; pause(); return; }

        std::cout << "\nAccount : " << acct->id() << "\n";
        std::cout << "Type    : " << acct->type_str() << "\n";
        std::cout << "Status  : " << acct->status_str() << "\n";
        std::cout << "Balance : " << acct->balance().to_string() << "\n";
        std::cout << "Interest: " << acct->interest_rate() * 100 << "% p.a.\n";
        pause();
    }

    void apply_interest_flow() {
        std::cout << "\n=== Applying Monthly Interest ===\n";
        int count = bank_.apply_interest_to_all_savings();
        std::cout << "✓ Interest applied to " << count << " accounts.\n";
        pause();
    }

    void undo_transaction_flow() {
        std::cout << "\n=== Undo Last Transaction ===\n";
        std::string aid = get_string("Account ID");
        bool ok = bank_.undo_last_transaction(aid);
        std::cout << (ok ? "✓ Undo successful.\n" : "✗ Nothing to undo.\n");
        pause();
    }

    void generate_reports_flow() {
        std::cout << "\n=== Full System Report ===\n";
        bank_.generate_full_report(std::cout);
        pause();
    }

    void benchmark_flow() {
        std::cout << "\n=== Hash Table Benchmark ===\n";
        int n = get_int("Number of elements to benchmark (e.g. 50000)");
        if (n <= 0 || n > 2000000) n = 50000;
        Benchmarker::compare((size_t)n);
        Benchmarker::print_memory_analysis();
        pause();
    }

    void save_load_flow() {
        std::cout << "\n=== Save / Load Database ===\n";
        std::cout << "1. Save\n2. Load\n";
        int opt = get_int("Option");
        if (opt == 1) {
            std::string path = get_string("File path (leave blank for default)");
            bool ok = bank_.save(path.empty() ? "" : path);
            std::cout << (ok ? "✓ Saved.\n" : "✗ Save failed.\n");
        } else {
            std::string path = get_string("File path (leave blank for default)");
            bool ok = bank_.load(path.empty() ? "" : path);
            std::cout << (ok ? "✓ Loaded.\n" : "✗ Load failed.\n");
        }
        pause();
    }
};

} // namespace IBS
