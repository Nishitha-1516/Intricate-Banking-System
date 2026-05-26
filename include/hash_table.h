#pragma once
#include <vector>
#include <string>
#include <optional>
#include <functional>
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cstdint>
#include <cmath>

namespace IBS {

// ============================================================
//  DoubleHashTable<K,V>
//  Open-addressing hash table with double hashing.
//  Primary:   h1(k) = polynomial_hash(k) % capacity
//  Secondary: h2(k) = prime - (polynomial_hash(k) % prime)
//  Probe:     (h1 + i*h2) % capacity
//
//  Guarantees:
//    - O(1) amortised insert / lookup / delete
//    - Load factor kept below 0.6 via dynamic resizing
//    - Collision statistics tracked per operation
// ============================================================
template<typename K, typename V>
class DoubleHashTable {
public:
    struct Stats {
        uint64_t insertions       = 0;
        uint64_t lookups          = 0;
        uint64_t deletions        = 0;
        uint64_t insert_collisions= 0;
        uint64_t lookup_collisions= 0;
        uint64_t resize_events    = 0;
        uint64_t total_probes     = 0;
        size_t   current_size     = 0;
        size_t   capacity         = 0;

        double load_factor()          const { return capacity ? (double)current_size/capacity : 0; }
        double insert_collision_rate()const { return insertions ? (double)insert_collisions/insertions : 0; }
        double lookup_collision_rate()const { return lookups    ? (double)lookup_collisions/lookups    : 0; }
        double avg_probe_len()        const { return (insertions+lookups) ? (double)total_probes/(insertions+lookups) : 0; }
    };

    explicit DoubleHashTable(size_t initial_capacity = (1 << 20)) // ~1M slots
        : capacity_(next_prime(initial_capacity))
        , table_(capacity_)
        , aux_prime_(largest_prime_below(capacity_))
    {
        stats_.capacity = capacity_;
    }

    // ---- Core operations ----
    bool insert(const K& key, const V& value);
    std::optional<V> find(const K& key) const;
    bool remove(const K& key);
    bool contains(const K& key) const { return find(key).has_value(); }
    void update(const K& key, const V& value);  // insert or overwrite

    // ---- Capacity / meta ----
    size_t size()     const { return stats_.current_size; }
    size_t capacity() const { return capacity_; }
    bool   empty()    const { return stats_.current_size == 0; }

    // ---- Statistics ----
    const Stats& stats() const { return stats_; }
    void print_stats(std::ostream& out = std::cout) const;
    void reset_stats();

    // ---- Iteration (for serialization / full scan) ----
    template<typename Fn>
    void for_each(Fn&& fn) const {
        for (const auto& slot : table_)
            if (slot.state == SlotState::OCCUPIED)
                fn(slot.key, slot.value);
    }

    // ---- Benchmark: compare vs linear probing ----
    void benchmark_vs_linear(size_t n_ops, std::ostream& out = std::cout);

private:
    enum class SlotState { EMPTY, OCCUPIED, DELETED };

    struct Slot {
        K         key{};
        V         value{};
        SlotState state = SlotState::EMPTY;
    };

    size_t           capacity_;
    std::vector<Slot> table_;
    size_t           aux_prime_;   // for h2
    mutable Stats    stats_;

    static constexpr double MAX_LOAD  = 0.60;
    static constexpr double MIN_LOAD  = 0.10;

    // ---- Hash functions ----
    static uint64_t polynomial_hash(const K& key);
    size_t h1(const K& key) const { return polynomial_hash(key) % capacity_; }
    size_t h2(const K& key) const {
        // Must be > 0 and coprime with capacity_ (prime guarantees coprimality)
        return aux_prime_ - (polynomial_hash(key) % aux_prime_);
    }
    size_t probe(const K& key, size_t i) const {
        return (h1(key) + i * h2(key)) % capacity_;
    }

    // ---- Resize ----
    void resize(size_t new_cap);
    void maybe_resize();

    // ---- Prime utilities ----
    static bool   is_prime(size_t n);
    static size_t next_prime(size_t n);
    static size_t largest_prime_below(size_t n);
};

// ============================================================
//  Specialised polynomial_hash for std::string
// ============================================================
template<typename K, typename V>
uint64_t DoubleHashTable<K,V>::polynomial_hash(const K& key) {
    // Bernstein-style polynomial hash with large prime multiplier
    // Works for any type via std::hash as fallback
    if constexpr (std::is_same_v<K, std::string>) {
        uint64_t hash = 14695981039346656037ULL; // FNV offset basis
        for (unsigned char c : key) {
            hash ^= c;
            hash *= 1099511628211ULL;  // FNV prime
        }
        return hash;
    } else {
        return std::hash<K>{}(key);
    }
}

template<typename K, typename V>
bool DoubleHashTable<K,V>::is_prime(size_t n) {
    if (n < 2) return false;
    if (n < 4) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (size_t i = 5; i * i <= n; i += 6)
        if (n % i == 0 || n % (i+2) == 0) return false;
    return true;
}

template<typename K, typename V>
size_t DoubleHashTable<K,V>::next_prime(size_t n) {
    if (n < 2) return 2;
    size_t candidate = (n % 2 == 0) ? n + 1 : n;
    while (!is_prime(candidate)) candidate += 2;
    return candidate;
}

template<typename K, typename V>
size_t DoubleHashTable<K,V>::largest_prime_below(size_t n) {
    if (n <= 2) return 2;
    size_t candidate = (n % 2 == 0) ? n - 1 : n - 2;
    while (!is_prime(candidate)) candidate -= 2;
    return candidate;
}

template<typename K, typename V>
bool DoubleHashTable<K,V>::insert(const K& key, const V& value) {
    maybe_resize();
    ++stats_.insertions;
    size_t first_deleted = SIZE_MAX;
    for (size_t i = 0; i < capacity_; ++i) {
        size_t idx = probe(key, i);
        ++stats_.total_probes;
        if (i > 0) ++stats_.insert_collisions;

        auto& slot = table_[idx];
        if (slot.state == SlotState::EMPTY) {
            size_t ins = (first_deleted != SIZE_MAX) ? first_deleted : idx;
            table_[ins] = {key, value, SlotState::OCCUPIED};
            ++stats_.current_size;
            return true;
        }
        if (slot.state == SlotState::DELETED && first_deleted == SIZE_MAX)
            first_deleted = idx;
        if (slot.state == SlotState::OCCUPIED && slot.key == key) {
            slot.value = value; // update
            return true;
        }
    }
    // Table full (shouldn't happen with resize)
    if (first_deleted != SIZE_MAX) {
        table_[first_deleted] = {key, value, SlotState::OCCUPIED};
        ++stats_.current_size;
        return true;
    }
    return false;
}

template<typename K, typename V>
std::optional<V> DoubleHashTable<K,V>::find(const K& key) const {
    ++stats_.lookups;
    for (size_t i = 0; i < capacity_; ++i) {
        size_t idx = probe(key, i);
        ++stats_.total_probes;
        if (i > 0) ++stats_.lookup_collisions;

        const auto& slot = table_[idx];
        if (slot.state == SlotState::EMPTY) return std::nullopt;
        if (slot.state == SlotState::OCCUPIED && slot.key == key)
            return slot.value;
    }
    return std::nullopt;
}

template<typename K, typename V>
bool DoubleHashTable<K,V>::remove(const K& key) {
    ++stats_.deletions;
    for (size_t i = 0; i < capacity_; ++i) {
        size_t idx = probe(key, i);
        auto& slot = table_[idx];
        if (slot.state == SlotState::EMPTY) return false;
        if (slot.state == SlotState::OCCUPIED && slot.key == key) {
            slot.state = SlotState::DELETED;
            --stats_.current_size;
            return true;
        }
    }
    return false;
}

template<typename K, typename V>
void DoubleHashTable<K,V>::update(const K& key, const V& value) {
    insert(key, value); // insert handles overwrite
}

template<typename K, typename V>
void DoubleHashTable<K,V>::resize(size_t new_cap) {
    ++stats_.resize_events;
    size_t nc = next_prime(new_cap);
    std::vector<Slot> old = std::move(table_);
    capacity_   = nc;
    aux_prime_  = largest_prime_below(nc);
    table_.assign(nc, Slot{});
    stats_.current_size = 0;
    stats_.capacity     = nc;
    for (auto& slot : old)
        if (slot.state == SlotState::OCCUPIED)
            insert(slot.key, slot.value);
}

template<typename K, typename V>
void DoubleHashTable<K,V>::maybe_resize() {
    double lf = (double)(stats_.current_size + 1) / capacity_;
    if (lf >= MAX_LOAD) resize(capacity_ * 2);
}

template<typename K, typename V>
void DoubleHashTable<K,V>::print_stats(std::ostream& out) const {
    out << "\n╔══════════════════════════════════════════════╗\n";
    out << "║       Double Hash Table Statistics           ║\n";
    out << "╠══════════════════════════════════════════════╣\n";
    out << std::fixed << std::setprecision(4);
    out << "║  Capacity          : " << std::setw(10) << capacity_           << "          ║\n";
    out << "║  Occupied slots    : " << std::setw(10) << stats_.current_size << "          ║\n";
    out << "║  Load factor       : " << std::setw(10) << stats_.load_factor()          << "          ║\n";
    out << "║  Insertions        : " << std::setw(10) << stats_.insertions   << "          ║\n";
    out << "║  Lookups           : " << std::setw(10) << stats_.lookups      << "          ║\n";
    out << "║  Deletions         : " << std::setw(10) << stats_.deletions    << "          ║\n";
    out << "║  Insert collisions : " << std::setw(10) << stats_.insert_collisions     << "          ║\n";
    out << "║  Lookup collisions : " << std::setw(10) << stats_.lookup_collisions     << "          ║\n";
    out << "║  Insert coll. rate : " << std::setw(10) << stats_.insert_collision_rate()*100 << " %        ║\n";
    out << "║  Lookup coll. rate : " << std::setw(10) << stats_.lookup_collision_rate()*100 << " %        ║\n";
    out << "║  Avg probe length  : " << std::setw(10) << stats_.avg_probe_len()       << "          ║\n";
    out << "║  Resize events     : " << std::setw(10) << stats_.resize_events         << "          ║\n";
    out << "╚══════════════════════════════════════════════╝\n";
}

template<typename K, typename V>
void DoubleHashTable<K,V>::reset_stats() {
    size_t sz  = stats_.current_size;
    size_t cap = capacity_;
    stats_ = Stats{};
    stats_.current_size = sz;
    stats_.capacity     = cap;
}

} // namespace IBS
