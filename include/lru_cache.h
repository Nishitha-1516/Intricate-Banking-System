#pragma once
#include <list>
#include <unordered_map>
#include <optional>
#include <mutex>
#include <cstddef>
#include <stdexcept>

namespace IBS {

// ============================================================
//  LRUCache<K,V>
//  Doubly-linked list + hash map, O(1) get/put.
//  Thread-safe via shared mutex.
// ============================================================
template<typename K, typename V>
class LRUCache {
public:
    explicit LRUCache(size_t capacity) : cap_(capacity) {
        if (cap_ == 0) throw std::invalid_argument("LRU capacity must be > 0");
    }

    // Returns value if present (and moves it to front)
    std::optional<V> get(const K& key) {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = map_.find(key);
        if (it == map_.end()) { ++misses_; return std::nullopt; }
        ++hits_;
        list_.splice(list_.begin(), list_, it->second);
        return it->second->second;
    }

    // Insert or update; evicts LRU entry if full
    void put(const K& key, const V& value) {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second->second = value;
            list_.splice(list_.begin(), list_, it->second);
            return;
        }
        if (list_.size() == cap_) {
            auto last = list_.back().first;
            map_.erase(last);
            list_.pop_back();
            ++evictions_;
        }
        list_.emplace_front(key, value);
        map_[key] = list_.begin();
    }

    bool contains(const K& key) const {
        std::lock_guard<std::mutex> lk(mtx_);
        return map_.count(key) > 0;
    }

    void invalidate(const K& key) {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = map_.find(key);
        if (it != map_.end()) {
            list_.erase(it->second);
            map_.erase(it);
        }
    }

    size_t size()     const { return map_.size(); }
    size_t capacity() const { return cap_; }

    // Stats
    uint64_t hits()      const { return hits_; }
    uint64_t misses()    const { return misses_; }
    uint64_t evictions() const { return evictions_; }
    double   hit_rate()  const {
        uint64_t total = hits_ + misses_;
        return total ? (double)hits_ / total : 0.0;
    }

    void clear() {
        std::lock_guard<std::mutex> lk(mtx_);
        list_.clear(); map_.clear();
    }

private:
    using ListIt = typename std::list<std::pair<K,V>>::iterator;
    size_t                           cap_;
    std::list<std::pair<K,V>>        list_;
    std::unordered_map<K, ListIt>    map_;
    mutable std::mutex               mtx_;
    uint64_t hits_ = 0, misses_ = 0, evictions_ = 0;
};

} // namespace IBS
