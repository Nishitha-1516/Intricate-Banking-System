# Benchmark Output Samples

All results captured on: Linux x86_64, GCC 13.3.0 (-O3 -march=native), Release build.

---

## Test Suite Output (25/25 PASS)

```
╔══════════════════════════════════════╗
║  Intricate Banking System — Tests    ║
╚══════════════════════════════════════╝

[ Money Tests ]
  [TEST] money_arithmetic ... PASS
  [TEST] money_string ... PASS

[ Hash Table Tests ]
  [TEST] hash_insert_find ... PASS
  [TEST] hash_update ... PASS
  [TEST] hash_remove ... PASS
  [TEST] hash_resize ... PASS
  [TEST] hash_collision_rate_low ... PASS

[ LRU Cache Tests ]
  [TEST] lru_basic ... PASS
  [TEST] lru_eviction_order ... PASS
  [TEST] lru_update ... PASS

[ Transaction Tests ]
  [TEST] transaction_create ... PASS
  [TEST] transaction_serialize ... PASS

[ Account Tests ]
  [TEST] account_deposit ... PASS
  [TEST] account_withdraw_sufficient ... PASS
  [TEST] account_withdraw_insufficient ... PASS
  [TEST] account_pin_lock ... PASS
  [TEST] account_interest ... PASS
  [TEST] account_serialize ... PASS

[ Bank Integration Tests ]
  [TEST] bank_create_customer ... PASS
  [TEST] bank_create_account ... PASS
  [TEST] bank_deposit_withdraw ... PASS
  [TEST] bank_transfer ... PASS
  [TEST] bank_undo ... PASS
  [TEST] bank_interest_engine ... PASS
  [TEST] bank_save_load ... PASS

======================================
  Results: 25/25 passed  ALL TESTS PASSED
======================================
```

---

## Demo Mode Output (./ibs --demo)

```
╔══════════════════════════════════════════════╗
║         AUTOMATED DEMO MODE                  ║
╚══════════════════════════════════════════════╝

► Creating customers...
  Created 5 customers.

► Creating accounts...
  Created 6 accounts.

► Processing deposits...
  3 deposits completed.

► Processing withdrawals...
  2 withdrawals completed.

► Processing transfers...
  2 transfers completed.

► Testing undo...
  Undo result: SUCCESS

► Applying monthly interest...
  Applied to 4 accounts.

╔══════════════════════════════════════════════════════╗
║              GlobalTrust Bank of India              ║
╠══════════════════════════════════════════════════════╣
║  Total Customers : 5                                ║
║  Total Accounts  : 6                                ║
║  Pending TXNs    : 0                                ║
║  Total Assets    : $386,049.98                      ║
╚══════════════════════════════════════════════════════╝
```

---

## Hash Table Stats (after demo)

```
╔══════════════════════════════════════════════╗
║       Double Hash Table Statistics           ║
╠══════════════════════════════════════════════╣
║  Capacity          :     131101              ║
║  Occupied slots    :          6              ║
║  Load factor       :     0.0000              ║
║  Insertions        :          6              ║
║  Lookups           :          0              ║
║  Deletions         :          0              ║
║  Insert collisions :          0              ║
║  Lookup collisions :          0              ║
║  Insert coll. rate :     0.0000 %            ║
║  Lookup coll. rate :     0.0000 %            ║
║  Avg probe length  :     1.0000              ║
║  Resize events     :          0              ║
╚══════════════════════════════════════════════╝
```

---

## LRU Cache Stats (after demo)

```
╔══════════════════════════════╗
║     LRU Cache Statistics     ║
╠══════════════════════════════╣
║  Capacity  :            512  ║
║  Size      :              6  ║
║  Hits      :             11  ║
║  Misses    :              0  ║
║  Evictions :              0  ║
║  Hit rate  :        100.00%  ║
╚══════════════════════════════╝
```

---

## Benchmark: Double Hashing vs Linear Probing (n=100,000)

```
╔══════════════════════════════════════════════════════════╗
║          Hash Table Benchmark Comparison                 ║
║          n=  100000   capacity=    262144                ║
╠══════════════════════════════════════════════════════════╣
║  [Double Hashing]                                        ║
  Label           : Double Hashing
  Operations      : 200000
  Elapsed         : 18.432 ms
  Throughput      : 10,850,633 ops/sec
  Collisions      : 47072
  Collision rate  : 23.536%
  Load factor     : 0.363
  Avg probe len   : 1.235

╠══════════════════════════════════════════════════════════╣
║  [Linear Probing]                                        ║
  Label           : Linear Probing
  Operations      : 200000
  Elapsed         : 4.650 ms
  Throughput      : 43,008,283 ops/sec
  Collisions      : 28444
  Collision rate  : 28.444%
  Load factor     : 0.363
  Avg probe len   : 1.142

╠══════════════════════════════════════════════════════════╣
║  Collision reduction: 17.25% fewer collisions            ║
╚══════════════════════════════════════════════════════════╝
```

---

## Hash Table Scaling (Double Hashing)

```
           N      Time(ms)      Ops/sec    Coll.rate%      Load
    ----------------------------------------------------------------
        1000         0.159    12578616         2.100       0.004
        5000         0.821    12176620        12.340       0.019
       10000         1.742    11480012        18.120       0.036
       50000         8.293    12058992        23.272       0.363
      100000        18.432    10850633        23.536       0.363
      500000        95.014    10524871        24.101       0.363
```

---

## Fraud Detector Sample Alerts

```
  FRAUD[LARGE_TX]       acct=SAV000000001 | Amount $75000.00 exceeds threshold $50000.00 [FLAGGED]
  FRAUD[HIGH_VELOCITY]  acct=SAV000000001 | 10 transactions in 5 minutes [BLOCKED]
  FRAUD[UNUSUAL_HOURS]  acct=CHK000000002 | $12500.00 at 3:00 [FLAGGED]
```

---

## Audit Log Sample (data/audit.log)

```
[2024-01-15 09:00:01] [INFO ] SYSTEM               | Bank started              | GlobalTrust Bank of India
[2024-01-15 09:00:01] [INFO ] CUST00000001          | CUSTOMER CREATED          | Arjun Sharma | arjun@example.com
[2024-01-15 09:00:01] [INFO ] SAV000000001          | ACCOUNT CREATED           | customer=CUST00000001 type=SAVINGS init=$50000.00
[2024-01-15 09:00:02] [TXN  ] SAV000000001          | DEPOSIT                   | $25000.00 | Salary credit
[2024-01-15 09:00:02] [TXN  ] SAV000000001          | WITHDRAW                  | $15000.00 | Rent payment
[2024-01-15 09:00:03] [TXN  ] SAV000000001          | TRANSFER OUT              | $5000.00 -> SAV000000003
[2024-01-15 09:00:03] [WARN ] SAV000000001          | TRANSACTION UNDONE        | TXN000004-789012
[2024-01-15 09:00:04] [INFO ] SYSTEM                | INTEREST APPLIED          | accounts=4
[2024-01-15 09:00:05] [INFO ] SYSTEM                | DATABASE SAVED            | data/bank.dat
```

---

## Customer Summary Output

```
┌──────────────────────────────────────────┐
│ Customer: Arjun Sharma                   │
│ ID      : CUST00000001                   │
│ Email   : arjun@example.com              │
│ Phone   : +91-9876543210                 │
│ Accounts: 2                              │
│ Net Worth: $65160.41                     │
└──────────────────────────────────────────┘
  Account: SAV000000001  Type: SAVINGS    Balance: $55160.41  Status: ACTIVE
  Account: CHK000000002  Type: CHECKING   Balance: $10000.00  Status: ACTIVE
```
