# Design Notes

Architecture decisions, data structure tradeoffs, and profiling results. Started this after going deep on [Coding Jesus](https://www.youtube.com/@CodingJesus)'s content on order books — the gap between "I understand what an order book is" and "I understand what makes one fast" turned out to be significant.

---

## Data Structures

### OrderBook

Each `OrderBook` holds two `std::map<Price, PriceLevel>` — bids (descending) and asks (ascending). `std::map` is a red-black tree: O(log n) insert and lookup where n is the number of distinct active price levels.

For most symbols under normal conditions, n stays small — 10–50 active levels. The log factor isn't the problem. The problem is `std::map` heap-allocates a node per price level, and that allocation shows up clearly in profiling.

Best bid and best ask are cached separately (`best_bid_`, `best_ask_`) so the matching engine checks for a cross in O(1) without touching the tree.

### PriceLevel: Intrusive doubly-linked list + hashmap index

Each price level is a FIFO queue of orders at that price. Intrusive doubly-linked list (each `OrderNode` carries `prev`/`next` directly) with a parallel `unordered_map<OrderId, OrderNode*>` for cancel lookups.

```
head → [OrderNode A] ↔ [OrderNode B] ↔ [OrderNode C] ← tail
                            ↑
               node_index_[order_id_B] ──────────┘
```

- **Front removal** (match happens): unlink `head`, erase from `node_index_`, delete node — O(1).
- **Cancel**: `node_index_.find(id)` → O(1) pointer → `unlink()` — O(1) total.

Before this fix, `PriceLevel` used a `std::vector<OrderNode*>` with a linear scan to find the target node. Cancel was O(n) in orders at that level. The measured improvement after switching to the hashmap index: **180K/s → ~4.5M/s** (25x).

### Order storage

The matching engine keeps all live orders in an `unordered_map<OrderId, Order>`. Used to validate cancels, update quantities on replace, and prevent self-trades. The Order struct is 64-byte aligned, so insertions trigger aligned `operator new`.

### SPSC Queue

Bounded ring buffer with two atomic indices (`head_`, `tail_`) and cache-line padding between them. Producer writes to `tail_`, consumer reads from `head_`. Single writer, single reader — no compare-and-swap needed, just `memory_order_acquire`/`release` loads and stores.

```
cache line 0: [head_] [padding...]    ← consumer reads this
cache line 1: [tail_] [padding...]    ← producer writes this
cache line 2+: [slot 0][slot 1]...    ← data
```

Keeping `head_` and `tail_` on separate cache lines prevents false sharing — the consumer doesn't invalidate the producer's cache line. Throughput: ~104M/s single-thread, ~11M/s cross-thread (the gap is cache coherence traffic between cores).

---

## Profiling Results

Profiled `AddOrder_NoMatch` using macOS `/usr/bin/sample` (8 seconds, 1ms intervals). This benchmark adds orders at fresh price levels every iteration — worst case for the tree.

**Call tree (6531 samples total):**

```
6531  AddOrder_NoMatch benchmark loop
  6046  exchange::OrderBook::add_order()
    5769  exchange::OrderBook::insert_resting_order()
      ├── 3876  std::map insert (std::__tree::__emplace_unique_key_args)
      │     ├── 2909  tree traversal / node insertion
      │     ├──  530  tree rebalancing (std::__tree_balance_after_insert)
      │     └──  438  operator new  ← heap alloc for each new tree node
      │
      └── 1103  std::unordered_map insert (order storage)
            ├──  616  hash table probe / rehash
            └──  416  operator new (aligned, 64-byte) ← alloc per Order object
```

The dominant cost is price level tree insertion. Every unique price level that doesn't already exist triggers a `new` inside `std::map`. Since this benchmark creates a fresh level per order, every single add hits the allocator.

Secondary cost is order storage — the `unordered_map<OrderId, Order>` also triggers aligned allocations on growth or 64-byte Order emplacement.

**IPC:** 330B instructions / 120B cycles = **2.73 IPC**. On M-series (capable of 4–5+ IPC when cache-hot), 2.73 suggests moderate cache pressure. The allocator calls are the main culprit — they touch cold memory.

**"Always match" runs at 47M/s** because matching never inserts into the tree — it just removes from the front of an existing level. No allocation, pure linked-list traversal. That's the ceiling. Real workloads land somewhere in between depending on order flow mix.

---

## Next Steps

### 1. Pool allocator for OrderNodes

Every cancel and partial fill calls `delete node`. Every new order calls `new OrderNode(...)`. Visible in the profile. Fix: pre-allocate a contiguous array of `OrderNode` objects, hand them out from a free list.

```cpp
struct NodePool {
    static constexpr size_t CAPACITY = 1 << 20; // 1M nodes
    std::array<OrderNode, CAPACITY> pool;
    std::vector<OrderNode*> free_list;

    NodePool() { for (auto& n : pool) free_list.push_back(&n); }
    OrderNode* alloc() { auto* p = free_list.back(); free_list.pop_back(); return p; }
    void free(OrderNode* p) { free_list.push_back(p); }
};
```

Removes `operator new` samples from PriceLevel. The `std::map` tree node allocations remain until the price level map itself is replaced.

### 2. Replace std::map with a flat hash map for price levels

`std::map` allocates a heap node per price level and pointer-chases through scattered memory. A flat hash map stores entries contiguously — cache-friendly, no per-entry allocation.

The tradeoff: `std::map` iterates in sorted order, which matters for walking bids/asks. A hash map doesn't. Options:

- Keep a separate sorted structure for iteration (rarely needed on the hot path)
- `std::flat_map` (C++23) — sorted, contiguous, but O(n) insert worst case
- Hash map for O(1) lookup + manually maintain `best_bid_`/`best_ask_`

This directly addresses the #1 bottleneck the profiler shows.

### 3. Fix get_bids() / get_asks() allocation

`get_bids()` and `get_asks()` currently return `std::vector<PriceLevel*>` built fresh on each call. Easy fixes:

- Return a view/span into internal data
- Pass a pre-allocated output buffer
- Expose an iterator range

### 4. Symbol sharding across threads

All symbols currently run on one matching thread. Sharding by symbol — AAPL to thread 0, GOOGL to thread 1, etc. — would scale throughput linearly with independent symbol count. Each shard gets its own SPSC queue and engine instance, no cross-shard synchronization needed.

Edge case: spread trades that leg across symbols. Real exchanges handle this with a separate spread book layer.

### 5. TCP order gateway

Everything runs in-process right now — agents submit by calling functions directly. A TCP gateway would let an external client (even a Python script) connect and submit orders, and would separate submission latency from matching latency. Minimal version: `epoll`-based server (Linux) or `kqueue` (macOS), binary order struct over the wire, fills streamed back.

### 6. Flamegraph on macOS

`sample` gives a text call tree. Instruments gives the same data as a visual flamegraph:

```bash
xctrace record --template 'Time Profiler' --launch -- ./build/benchmarks \
    --benchmark_filter="AddOrder_NoMatch" --benchmark_min_time=8s
# open the .trace file in Instruments.app
```

Linux: `perf record -g ./benchmarks && perf script | stackcollapse-perf.pl | flamegraph.pl > flame.svg`

---

## Threading Model

Currently single-threaded matching with a simulated SPSC queue — the queue exists but producer and consumer run sequentially in the main loop. Full multi-threaded operation:

```
Thread 0 (agents): agent.tick() → spsc_queue.push(order)
Thread 1 (engine): while (true) { spsc_queue.pop(order); engine.process(order); }
```

The queue supports true two-thread operation today — agents just run synchronously for simplicity.

---

## Benchmark Notes

Numbers vary with machine load and CPU frequency scaling. For stable readings:

- Pin to a single core: `taskset -c 0 ./benchmarks` (Linux only — macOS doesn't have `taskset`)
- Build with `-O3 -march=native` (the Release preset does this)
- No other CPU-intensive processes running
- `--benchmark_min_time=3s` for tighter confidence intervals

"AddOrder always match" (47M/s) is an upper bound — fills from a pre-populated book with one price level, so no tree insertion and the cache stays hot. Real workloads sit between that and the "no match" case.
