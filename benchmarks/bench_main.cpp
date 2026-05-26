// ============================================================
//  Intricate Banking System — Standalone Benchmark Runner
//  Compile: cmake --build . && ./bin/ibs_bench
// ============================================================
#include "benchmarker.h"
#include "hash_table.h"
#include "lru_cache.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <numeric>

using namespace IBS;

void print_separator() {
    std::cout << "\n" << std::string(62, '═') << "\n";
}

void benchmark_hash_scaling() {
    std::cout << "\n>>> Hash Table Scaling Benchmark\n";
    std::cout << std::setw(12) << "N"
              << std::setw(14) << "Time(ms)"
              << std::setw(14) << "Ops/sec"
              << std::setw(14) << "Coll.rate%"
              << std::setw(10) << "Load"
              << "\n";
    std::cout << std::string(64, '-') << "\n";

    std::vector<size_t> ns = {1000, 5000, 10000, 50000, 100000, 500000};
    for (size_t n : ns) {
        auto r = Benchmarker::bench_double_hash(n);
        std::cout << std::fixed << std::setprecision(3)
                  << std::setw(12) << n
                  << std::setw(14) << r.elapsed_ms
                  << std::setw(14) << r.ops_per_sec
                  << std::setw(14) << r.collision_rate * 100
                  << std::setw(10) << r.load_factor
                  << "\n";
    }
}

void benchmark_lru_cache() {
    std::cout << "\n>>> LRU Cache Benchmark (capacity=1000, ops=100000)\n";
    LRUCache<std::string, int> cache(1000);

    auto ids = Benchmarker::gen_random_ids(2000);  // 2x capacity → mix of hits/misses

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100000; ++i) {
        const auto& key = ids[i % 2000];
        if (i % 3 == 0) cache.put(key, i);
        else            cache.get(key);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1-t0).count();

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "  Elapsed  : " << ms << " ms\n";
    std::cout << "  Throughput: " << (size_t)(100000 / (ms/1000)) << " ops/sec\n";
    std::cout << "  Hit rate : " << cache.hit_rate() * 100 << " %\n";
    std::cout << "  Evictions: " << cache.evictions() << "\n";
}

void benchmark_double_vs_linear_sweep() {
    std::cout << "\n>>> Double Hashing vs Linear Probing (varying load factor)\n";
    std::cout << std::setw(8)  << "n"
              << std::setw(10) << "cap"
              << std::setw(16) << "DH_time(ms)"
              << std::setw(16) << "LP_time(ms)"
              << std::setw(14) << "DH_coll%"
              << std::setw(14) << "LP_coll%"
              << "\n";
    std::cout << std::string(78, '-') << "\n";

    size_t cap = 1 << 15; // 32768 fixed capacity
    std::vector<size_t> ns = {5000, 10000, 15000, 18000, 19000};
    for (size_t n : ns) {
        auto r1 = Benchmarker::bench_double_hash(n, cap);
        auto r2 = Benchmarker::bench_linear_probe(n, cap);
        std::cout << std::fixed << std::setprecision(3)
                  << std::setw(8)  << n
                  << std::setw(10) << cap
                  << std::setw(16) << r1.elapsed_ms
                  << std::setw(16) << r2.elapsed_ms
                  << std::setw(14) << r1.collision_rate*100
                  << std::setw(14) << r2.collision_rate*100
                  << "\n";
    }
}

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║       Intricate Banking System — Full Benchmark          ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";

    benchmark_hash_scaling();
    print_separator();
    Benchmarker::compare(100000);
    print_separator();
    benchmark_double_vs_linear_sweep();
    print_separator();
    benchmark_lru_cache();
    print_separator();
    Benchmarker::print_memory_analysis();

    std::cout << "\nBenchmark complete.\n";
    return 0;
}
