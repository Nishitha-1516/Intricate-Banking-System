# Architecture & Design Decisions

## Core Design Principles

### 1. Separation of Concerns
Each class has a single, well-defined responsibility:
- `Money` — value type for currency (integer cents, no float errors)
- `Transaction` — immutable event record (factory construction, state machine)
- `Account` — balance owner with security and history
- `Customer` — identity owner, aggregates accounts
- `Bank` — orchestrator, owns indices and infrastructure
- `FraudDetector` — stateless rule evaluator with mutable velocity windows
- `AuditLogger` — cross-cutting concern, singleton

### 2. Integer Money Arithmetic
Floating-point arithmetic is never used for balances. All amounts are stored as `int64_t cents`. This eliminates:
- Rounding errors (e.g., $0.1 + $0.2 ≠ $0.3 in float)
- Representation drift over many transactions
- Non-deterministic comparisons

### 3. Immutable Transaction Records
Transactions are created via a factory (`Transaction::create`) and are immutable post-creation except for their `status` field (PENDING → COMPLETED/FAILED/ROLLED_BACK). This models the real-world ledger principle: transactions are never deleted, only superseded.

### 4. Deadlock-Free Transfers
Transferring between two accounts requires locking both. Naive locking in acquisition order leads to deadlock:
```
Thread A: lock(acc1), lock(acc2)
Thread B: lock(acc2), lock(acc1)  → deadlock
```
Solution: always acquire locks in pointer address order using `std::lock()`:
```cpp
std::mutex* first  = (this < &dest) ? &mtx_ : &dest.mtx_;
std::mutex* second = (this < &dest) ? &dest.mtx_ : &mtx_;
std::lock(*first, *second);
```
This is a well-known total-ordering deadlock prevention technique.

### 5. LRU as a Read-Through Cache
The LRU cache sits in front of the hash table. On a cache miss, the hash table is consulted and the result is promoted to cache. This means:
- Hot accounts (recently accessed) are served in O(1) with zero hash computation
- Cold accounts fall through to the hash table (still O(1) amortized)
- The cache is invalidated on account close/delete

### 6. Thread Pool Over Per-Request Threads
Creating a new `std::thread` per transaction has ~10–50μs overhead per transaction. A fixed-size pool (4 threads) eliminates this overhead and bounds memory usage regardless of request rate.

### 7. Prime-Sized Hash Table Capacity
Both the table capacity and the auxiliary hash prime must be prime numbers. This guarantees:
- `gcd(h2(k), capacity) = 1` for all keys k
- Every slot is reachable within `capacity` probes (complete probe sequence)
- No clustering patterns that repeat with period < capacity

## Key Tradeoffs

### Hash Table: Template Header-Only
`DoubleHashTable<K,V>` is fully template-implemented in the header. This means:
- ✅ No separate compilation needed for each specialization
- ✅ Compiler can inline and optimize aggressively
- ❌ Larger compile times
- ❌ Implementation detail exposed

### Text Serialization Over Binary
- ✅ Human-readable for debugging
- ✅ Forward-compatible (new fields can be added)
- ❌ Slower (3–5x) than binary for large datasets

### FNV-1a Over SHA/MD5 for PIN Hashing
- ✅ Fast (no crypto overhead needed for demo purposes)
- ✅ Avalanche effect sufficient for obfuscation
- ❌ Not cryptographically secure (use bcrypt/Argon2 in production)

## Extension Points

| Feature | How to Extend |
|---|---|
| Scheduled payments | Add `ScheduledPayment` struct + background timer thread in `Bank` |
| Multi-bank transfers | Add `BankNetwork` class with inter-bank settlement |
| Database backend | Replace `fstream` serialization with SQLite or PostgreSQL adapter |
| REST API | Add HTTP layer (e.g., cpp-httplib) calling `Bank` methods |
| Crypto PIN | Replace `hash_pin` with libsodium `crypto_pwhash` |
| Distributed | Replace `DoubleHashTable` with distributed KV store client |
