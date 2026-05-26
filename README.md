# Intricate Banking System

A production-grade **C++17 Bank Database Management System** built from scratch — demonstrating advanced systems programming, custom data structures, multithreaded transaction processing, and real-time fraud detection. Designed as a portfolio/resume project for top-tier software engineering roles.

---

## Table of Contents
- [Architecture Overview](#architecture-overview)
- [Features](#features)
- [Project Structure](#project-structure)
- [Build Instructions](#build-instructions)
- [Usage](#usage)
- [Technical Design](#technical-design)
- [Complexity Analysis](#complexity-analysis)
- [Benchmark Results](#benchmark-results)
- [Advanced Features](#advanced-features)

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                     AdminCLI (main.cpp)                  │
│              Menu-driven terminal dashboard              │
└─────────────────┬───────────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────────┐
│                     Bank (Orchestrator)                  │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────┐ │
│  │ Thread Pool  │  │  LRU Cache   │  │FraudDetector  │ │
│  │  (4 threads) │  │  (512 slots) │  │  (5 rules)    │ │
│  └──────────────┘  └──────────────┘  └───────────────┘ │
│  ┌──────────────────────────────────────────────────┐   │
│  │       DoubleHashTable<AccountID, Account>         │   │
│  │  Primary: FNV-1a % capacity                       │   │
│  │  Secondary: prime - (FNV-1a % prime)              │   │
│  │  Open addressing, ~1M capacity, resize at 60% LF  │   │
│  └──────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
         │               │               │
┌────────▼──────┐ ┌──────▼──────┐ ┌─────▼──────────┐
│   Customer    │ │   Account   │ │  Transaction   │
│ - ID, name    │ │ - balance   │ │ - unique ID    │
│ - email/phone │ │ - PIN hash  │ │ - timestamp    │
│ - accounts[]  │ │ - history[] │ │ - type/status  │
└───────────────┘ └─────────────┘ └────────────────┘
         │
┌────────▼──────────────────────────────────────────┐
│              Persistent Storage (fstream)          │
│   data/bank.dat  — custom text serialization       │
│   data/audit.log — append-only structured log      │
└───────────────────────────────────────────────────┘
```

---

## Features

### Core Banking
| Feature | Description |
|---|---|
| Customer CRUD | Create, read, search, delete customers with email/ID lookup |
| Account management | SAVINGS, CHECKING, FIXED_DEPOSIT account types |
| Deposit / Withdraw | Atomic, mutex-protected balance updates |
| Transfer | Deadlock-safe dual-lock transfer between any two accounts |
| Balance inquiry | O(1) lookup via LRU cache + hash table |
| Transaction history | Full per-account log with timestamps |

### Data Structures (implemented from scratch)
| Structure | File | Use Case |
|---|---|---|
| `DoubleHashTable<K,V>` | `include/hash_table.h` | O(1) account index |
| `LRUCache<K,V>` | `include/lru_cache.h` | Hot-path account cache |
| Undo stack | `src/bank.cpp` | Per-account undo/redo |
| Transaction deque | `src/fraud_detector.cpp` | Velocity window tracking |

### Advanced Features
- **Multithreaded transaction processing** — 4-thread worker pool with condition variable queue
- **LRU Cache** — O(1) get/put, doubly-linked list + unordered_map
- **Audit logging** — append-only structured log (5 levels: INFO/WARN/ERROR/SEC/TXN)
- **Undo/redo stack** — per-account rollback of last transaction
- **Interest calculation engine** — monthly compound interest for SAVINGS and FIXED_DEPOSIT
- **Fraud detection** — 5 real-time rules (large tx, velocity, unusual hours, rapid withdrawals, failed logins)
- **PIN security** — FNV-1a + salt hash, 3-strike account locking
- **Binary serialization** — full save/load of bank state

---

## Project Structure

```
Intricate-Banking-System/
├── include/
│   ├── types.h           # Money, enums, core type aliases
│   ├── transaction.h     # Transaction class + factory
│   ├── account.h         # Account with security & interest
│   ├── customer.h        # Customer owning multiple accounts
│   ├── hash_table.h      # DoubleHashTable<K,V> (template, header-only)
│   ├── lru_cache.h       # LRUCache<K,V> (template, header-only)
│   ├── audit_logger.h    # Singleton append-only logger
│   ├── fraud_detector.h  # Rule-based fraud engine
│   ├── bank.h            # Top-level orchestrator
│   ├── admin_cli.h       # Menu-driven terminal UI
│   └── benchmarker.h     # Performance measurement utilities
├── src/
│   ├── main.cpp          # Entry point (interactive / --demo / --bench)
│   ├── transaction.cpp
│   ├── account.cpp
│   ├── customer.cpp
│   ├── fraud_detector.cpp
│   ├── bank.cpp
│   └── benchmarker.cpp
├── tests/
│   └── test_suite.cpp    # 25 unit + integration tests
├── benchmarks/
│   └── bench_main.cpp    # Standalone benchmark runner
├── data/                 # Runtime: bank.dat, audit.log
├── docs/                 # Additional documentation
├── CMakeLists.txt
└── README.md
```

---

## Build Instructions

### Prerequisites
- CMake >= 3.16
- GCC >= 9 or Clang >= 10 (C++17 support required)
- POSIX threads (Linux/macOS) or Windows threads

### Build

```bash
git clone https://github.com/yourname/Intricate-Banking-System.git
cd Intricate-Banking-System
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j4
```

This produces three binaries in `build/bin/`:

| Binary | Purpose |
|---|---|
| `ibs` | Interactive CLI or demo/bench mode |
| `ibs_tests` | Full test suite (25 tests) |
| `ibs_bench` | Standalone hash table benchmarks |

### Run

```bash
# Interactive CLI
./build/bin/ibs

# Automated demo (creates accounts, runs transactions, shows reports)
./build/bin/ibs --demo

# Standalone benchmark (n elements)
./build/bin/ibs --bench 100000

# Full test suite
./build/bin/ibs_tests

# Comprehensive benchmark runner
./build/bin/ibs_bench
```

---

## Usage — Interactive CLI

```
┌─────────────────────────────────────┐
│          ADMIN DASHBOARD            │
├─────────────────────────────────────┤
│  1.  Create Account                 │
│  2.  Delete Account / Customer      │
│  3.  Search Customer                │
│  4.  Deposit / Withdraw / Transfer  │
│  5.  View Transaction History       │
│  6.  Balance Inquiry                │
│  7.  Apply Monthly Interest         │
│  8.  Undo Last Transaction          │
│  9.  Generate Full Report           │
│  10. Hash Table Benchmark           │
│  11. Save / Load Database           │
│  0.  Exit                           │
└─────────────────────────────────────┘
```

State is auto-saved on exit and auto-loaded on startup from `data/bank.dat`.

---

## Technical Design

### Custom Double Hashing

The hash table is implemented entirely from scratch in `include/hash_table.h`.

**Hash functions:**
```
h1(k) = FNV-1a(k) % capacity          (primary)
h2(k) = aux_prime - (FNV-1a(k) % aux_prime)   (secondary, always > 0)
probe(k, i) = (h1(k) + i * h2(k)) % capacity
```

**Why FNV-1a?**
- 64-bit output with excellent avalanche effect
- No multiplication needed; very cache-friendly
- Empirically low collision rate for string keys (account IDs)

**Why double hashing over linear/quadratic?**
- Linear probing: primary clustering — long runs form, O(n) worst case
- Quadratic probing: secondary clustering, misses some slots
- Double hashing: pseudo-random stride eliminates clustering, guaranteed to visit all slots when capacity is prime

**Resizing:** When load factor ≥ 0.60, the table is rebuilt at `next_prime(2 * capacity)`. All existing entries are re-inserted (rehashed).

### LRU Cache

Implemented with a `std::list<pair<K,V>>` (doubly-linked, O(1) splice) and an `unordered_map<K, ListIterator>` for O(1) lookup. Thread-safe via `std::mutex`.

- `get(k)`: lookup in map → splice node to front → return value → O(1)
- `put(k,v)`: if full, erase map entry for list.back(), pop_back, push_front → O(1)

### Concurrent Transaction Processing

The `Bank` maintains a 4-thread worker pool:
- A `std::queue<TransactionRequest>` protected by `std::mutex`
- Workers sleep on `std::condition_variable`, wake on new request
- Account-level `std::mutex` ensures consistent balance updates
- Transfer uses ordered dual-lock (`std::lock(m1, m2)`) to prevent deadlock

### PIN Security

PINs are never stored in plaintext. The storage format is:

```
stored = FNV-1a( "IBS_SALT_2024_$#@!" + raw_pin + "IBS_SALT_2024_$#@!" )
```

Output is a 16-character hex string. After 3 failed verifications, the account status transitions to `LOCKED` and is recorded in the audit log.

### Fraud Detection Rules

| Rule | Trigger | Action |
|---|---|---|
| `LARGE_TX` | Amount > $50,000 | FLAG |
| `HIGH_VELOCITY` | > 10 tx in 5 min | BLOCK |
| `UNUSUAL_HOURS` | 2–5 AM, amount > $10k | FLAG |
| `RAPID_WITHDRAWALS` | > 5 withdrawals in 60s | BLOCK |
| `FAILED_LOGINS` | ≥ 2 failed PINs | FLAG/BLOCK |

### Serialization Format

The persistence format is human-readable and section-delimited for easy debugging:

```
BANK:GlobalTrust Bank of India
CUSTOMERS:5
CUSTOMER_BEGIN
CUST00000001
Arjun
...
ACCOUNTS:2
ACCOUNT_BEGIN
SAV000000001
...
TXN_BEGIN
TXN000001-123456|0|...|...|500000|1|1716000000000|Salary credit
TXN_END
ACCOUNT_END
CUSTOMER_END
```

---

## Complexity Analysis

| Operation | Time | Space | Notes |
|---|---|---|---|
| `create_customer` | O(1) | O(1) | HashMap insert |
| `create_account` | O(1) amortized | O(1) | DH insert + LRU put |
| `find_account` | O(1) | — | LRU hit → O(1); miss → DH lookup O(1) |
| `deposit / withdraw` | O(1) | O(1) | Mutex lock + balance update |
| `transfer` | O(1) | O(1) | Ordered dual-lock |
| `transaction history` | O(n) | — | n = history length |
| `apply_interest` | O(A) | — | A = total accounts |
| `save` | O(C * A * T) | — | C=customers, A=accounts, T=avg tx |
| `load` | O(C * A * T) | O(C * A * T) | Full deserialization |
| DH `insert` | O(1) amortized | — | Resize at LF=0.60 |
| DH `find` | O(1) amortized | — | Avg probe < 1.3 at LF=0.36 |
| LRU `get` | O(1) | — | list splice + map lookup |
| LRU `put` | O(1) | — | O(1) eviction |

**Space complexity:** O(C + A + T) where C = customers, A = accounts, T = total transactions.

---

## Benchmark Results

Run on: Linux x86-64, GCC 13.3, Release build (-O3)

### Hash Table Scaling (Double Hashing)

| N | Time (ms) | Throughput (ops/s) | Collision % | Load Factor |
|---|---|---|---|---|
| 1,000 | 0.2 | ~10M | 5.2% | 0.03 |
| 10,000 | 1.8 | ~11M | 18.1% | 0.08 |
| 50,000 | 8.3 | ~12M | 23.3% | 0.36 |
| 100,000 | 18.4 | ~10.9M | 23.5% | 0.36 |
| 500,000 | 95.1 | ~10.5M | 24.1% | 0.36 |

### Double Hashing vs Linear Probing (n=100,000)

| Metric | Double Hashing | Linear Probing |
|---|---|---|
| Collision rate | **23.5%** | 28.4% |
| Avg probe length | **1.24** | 1.14 |
| Throughput | 10.9M ops/s | 43M ops/s |

> Linear probing has higher raw throughput due to cache locality, but double hashing achieves **17–18% fewer collisions** — critical for systems with high load factors.

### LRU Cache (capacity=1000, 100K mixed ops)

| Metric | Value |
|---|---|
| Throughput | ~45M ops/sec |
| Hit rate | ~67% (2x capacity key space) |
| Evictions | ~33K |

### Memory Usage

| Component | Per-unit size | 1M accounts |
|---|---|---|
| Hash table slot | ~88 bytes | ~88 MB |
| Account object | ~200 bytes | ~200 MB |
| LRU node | ~88 bytes | ~88 MB |
| Transaction | ~112 bytes | varies |

---

## Advanced Features Implemented

1. **Multithreaded transaction processing** — 4-thread pool, `condition_variable` signaling, `std::lock()` deadlock prevention
2. **LRU cache** — O(1) doubly-linked list + hash map, thread-safe, hit-rate tracking
3. **Audit logging system** — singleton, thread-safe, 5 severity levels, flush-on-write
4. **Undo/redo transaction stack** — per-account `std::stack<Transaction>` with balance reversal
5. **Interest calculation engine** — monthly compound interest for SAVINGS and FIXED_DEPOSIT accounts
6. **Fraud detection** — 5 real-time rule engine with blocking/flagging and per-account velocity windows
7. **Concurrent account access** — all account mutations protected by `std::mutex`, transfers use ordered dual-lock

---

## License

MIT License — free to use as a portfolio/learning reference.
