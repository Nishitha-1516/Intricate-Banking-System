#pragma once
#include "hash_table.h"
#include <string>
#include <vector>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <random>
#include <functional>

namespace IBS {

// ============================================================
//  Benchmarker — measures hash table and system performance
// ============================================================
class Benchmarker {
public:
    struct Result {
        std::string label;
        size_t      n_ops;
        double      elapsed_ms;
        double      ops_per_sec;
        uint64_t    collisions;
        double      collision_rate;
        double      load_factor;
        double      avg_probe_len;

        void print(std::ostream& out = std::cout) const;
    };

    // Run insert + lookup benchmark on DoubleHashTable
    static Result bench_double_hash(size_t n, size_t capacity = (1 << 17));

    // Simulate linear probing and measure collisions for comparison
    static Result bench_linear_probe(size_t n, size_t capacity = (1 << 17));

    // Full comparison report
    static void compare(size_t n, std::ostream& out = std::cout);

    // Memory usage estimate
    static void print_memory_analysis(std::ostream& out = std::cout);

    // Generate random account-like IDs
    static std::vector<std::string> gen_random_ids(size_t n, unsigned seed = 42);

private:
    // Simple linear probing table (for comparison only)
    struct LinearTable {
        struct Slot { std::string key; bool occupied = false; bool deleted = false; };
        size_t capacity;
        std::vector<Slot> table;
        uint64_t collisions = 0;
        size_t   size       = 0;

        explicit LinearTable(size_t cap) : capacity(cap), table(cap) {}
        bool insert(const std::string& key);
        bool find  (const std::string& key) const;
        double load_factor() const { return (double)size / capacity; }
    };
};

} // namespace IBS
