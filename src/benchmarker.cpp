#include "benchmarker.h"
#include <iostream>
#include <iomanip>
#include <numeric>
#include <cmath>

namespace IBS {

std::vector<std::string> Benchmarker::gen_random_ids(size_t n, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<uint64_t> dist;
    std::vector<std::string> ids;
    ids.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        std::ostringstream ss;
        ss << "ACC" << std::setfill('0') << std::setw(12) << dist(rng) % (n * 10 + 1);
        ids.push_back(ss.str());
    }
    return ids;
}

void Benchmarker::Result::print(std::ostream& out) const {
    out << std::fixed << std::setprecision(3);
    out << "  Label           : " << label        << "\n";
    out << "  Operations      : " << n_ops         << "\n";
    out << "  Elapsed         : " << elapsed_ms    << " ms\n";
    out << "  Throughput      : " << ops_per_sec   << " ops/sec\n";
    out << "  Collisions      : " << collisions    << "\n";
    out << "  Collision rate  : " << std::setprecision(4) << collision_rate * 100 << " %\n";
    out << "  Load factor     : " << load_factor   << "\n";
    out << "  Avg probe len   : " << avg_probe_len << "\n";
}

Benchmarker::Result Benchmarker::bench_double_hash(size_t n, size_t capacity) {
    auto ids = gen_random_ids(n);
    DoubleHashTable<std::string, int> ht(capacity);
    ht.reset_stats();

    auto t0 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < n; ++i) ht.insert(ids[i], (int)i);
    for (size_t i = 0; i < n; ++i) ht.find(ids[i]);
    auto t1 = std::chrono::high_resolution_clock::now();

    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    const auto& s = ht.stats();
    return Result{
        "Double Hashing",
        n * 2,
        ms,
        (n * 2) / (ms / 1000.0),
        s.insert_collisions + s.lookup_collisions,
        s.insert_collision_rate(),
        s.load_factor(),
        s.avg_probe_len()
    };
}

// ---- Simple linear probing table ----
bool Benchmarker::LinearTable::insert(const std::string& key) {
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : key) { hash ^= c; hash *= 1099511628211ULL; }
    size_t idx = hash % capacity;
    size_t probes = 0;
    while (table[idx].occupied && table[idx].key != key) {
        idx = (idx + 1) % capacity;
        ++probes;
        ++collisions;
    }
    if (!table[idx].occupied) { table[idx] = {key, true, false}; ++size; }
    return true;
}

bool Benchmarker::LinearTable::find(const std::string& key) const {
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : key) { hash ^= c; hash *= 1099511628211ULL; }
    size_t idx = hash % capacity;
    while (table[idx].occupied || table[idx].deleted) {
        if (table[idx].occupied && table[idx].key == key) return true;
        idx = (idx + 1) % capacity;
    }
    return false;
}

Benchmarker::Result Benchmarker::bench_linear_probe(size_t n, size_t capacity) {
    auto ids = gen_random_ids(n);
    LinearTable lt(capacity);

    auto t0 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < n; ++i) lt.insert(ids[i]);
    for (size_t i = 0; i < n; ++i) lt.find(ids[i]);
    auto t1 = std::chrono::high_resolution_clock::now();

    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double cr = n > 0 ? (double)lt.collisions / n : 0;
    return Result{
        "Linear Probing",
        n * 2,
        ms,
        (n * 2) / (ms / 1000.0),
        lt.collisions,
        cr,
        lt.load_factor(),
        1.0 + cr / 2.0  // approximate
    };
}

void Benchmarker::compare(size_t n, std::ostream& out) {
    size_t cap = 1;
    while (cap < n * 2) cap <<= 1;  // next power of 2 > 2n

    out << "\n╔══════════════════════════════════════════════════════════╗\n";
    out << "║          Hash Table Benchmark Comparison                 ║\n";
    out << "║          n=" << std::setw(8) << n << "   capacity=" << std::setw(10) << cap << "              ║\n";
    out << "╠══════════════════════════════════════════════════════════╣\n";

    out << "║  [Double Hashing]                                        ║\n";
    auto r1 = bench_double_hash(n, cap);
    r1.print(out);

    out << "╠══════════════════════════════════════════════════════════╣\n";
    out << "║  [Linear Probing]                                        ║\n";
    auto r2 = bench_linear_probe(n, cap);
    r2.print(out);

    out << "╠══════════════════════════════════════════════════════════╣\n";
    out << "║  Speedup (DH vs LP):  ";
    double speedup = r2.elapsed_ms / (r1.elapsed_ms + 1e-9);
    out << std::fixed << std::setprecision(2) << std::setw(6) << speedup << "x faster                   ║\n";
    out << "║  Collision reduction: ";
    double cr_red = (r2.collision_rate - r1.collision_rate) / (r2.collision_rate + 1e-9) * 100;
    out << std::setw(6) << cr_red << "% fewer collisions           ║\n";
    out << "╚══════════════════════════════════════════════════════════╝\n";
}

void Benchmarker::print_memory_analysis(std::ostream& out) {
    out << "\n╔══════════════════════════════════════════════════╗\n";
    out << "║           Memory Usage Analysis                  ║\n";
    out << "╠══════════════════════════════════════════════════╣\n";

    // Approximate sizes
    size_t slot_size = sizeof(std::string) * 2 + sizeof(int) + 1; // key+value+state
    size_t capacities[] = {1<<10, 1<<17, 1<<20};
    for (auto cap : capacities) {
        double mb = (double)(cap * slot_size) / (1024*1024);
        out << "║  capacity=" << std::setw(8) << cap
            << "  ~" << std::fixed << std::setprecision(2) << std::setw(8) << mb << " MB          ║\n";
    }

    out << "╠══════════════════════════════════════════════════╣\n";
    out << "║  LRUCache node (key+val+ptrs) : ~" << std::setw(6)
        << (sizeof(std::string)*2 + 3*8) << " bytes        ║\n";
    out << "║  Transaction object           : ~" << std::setw(6)
        << sizeof(double)*4 + 80 << " bytes        ║\n";
    out << "║  Account (base, no history)   : ~" << std::setw(6)
        << 200 << " bytes        ║\n";
    out << "╚══════════════════════════════════════════════════╝\n";
}

} // namespace IBS
