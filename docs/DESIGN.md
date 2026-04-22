# Design Deep-Dive

Architecture decisions, data structure tradeoffs, profiling results, and what to do next.

---

## Data Structures

### OrderBook: Two sides, sorted by price

Each `OrderBook` holds two `std::map<Price, PriceLevel>` — one for bids (descending), one for asks (ascending). `std::map` is a red-black tree, which gives O(log n) insert and lookup where n is the number of distinct price levels active at once.

For most symbols in normal market conditions, n stays small — maybe 10–50 active levels. The log factor doesn't matter much there. What does matter is that `std::map` heap-allocates a node for each price level, and that allocation shows up in profiling.

The best bid and best ask are cached separately (`best_bid_`, `best_ask_`) so the matching engine can check for a cross in O(1) without touching the tree.

### PriceLevel: Intrusive doubly-linked list + hashmap index

Each price level is a FIFO queue of orders waiting at that price. The data structure is an intrusive doubly-linked list (each `OrderNode` has `prev`/`next` pointers stored directly in the node) with a parallel `unordered_map<OrderId, OrderNode*>` for cancel lookups.

```
head → [OrderNode A] ↔ [OrderNode B] ↔ [OrderNode C] ← tail
                            ↑
               node_index_[order_id_B] ──────────┘
```

**Front removal** (when a match happens): unlink `head`, erase from `node_index_`, delete node — O(1).

**Cancel** (order withdrawn before matching): `node_index_.find(id)` → O(1) pointer → `unlink()` → O(1) total.

Before the cancel fix, `PriceLevel` used a `std::vector<OrderNode*>` and scanned linearly to find the node. Cancel was O(n) in the number of orders at that price level. With the `unordered_map` index, it's O(1). The measured improvement: **180K/s → ~4.5M/s** (25x).

### Order storage: unordered_map<OrderId, Order>

The matching engine keeps every live order in an `unordered_map<OrderId, Order>`. This lets it validate cancels, update quantities on replace, and prevent self-trades. The Order struct is 64-byte aligned, so insertions trigger aligned `operator new` calls.

### SPSC Queue

A bounded ring buffer with two atomic indices (`head_`, `tail_`) and cache-line padding between them. The producer thread writes to `tail_`; the consumer reads from `head_`. Because there's only one writer and one reader, no compare-and-swap is needed — just load and store with `memory_order_acquire`/`release`.

```
cache line 0: [head_] [padding...]    ← consumer reads this
cache line 1: [tail_] [padding...]    ← producer writes this
cache line 2+: [slot 0][slot 1]...    ← data
```

Keeping `head_` and `tail_` on separate cache lines prevents false sharing: the consumer doesn't invalidate the producer's cache line and vice versa. Throughput: ~104M/s single-thread, ~11M/s cross-thread (the gap is cache coherence traffic between cores).

---

## Profiling Results

Profiled `AddOrder_NoMatch` benchmark using macOS `/usr/bin/sample` (8 seconds, 1ms intervals). This benchmark repeatedly adds orders at fresh price levels — the worst case for the tree.

**Summary of the call tree (6531 samples total):**

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

**What this tells us:**

The dominant cost in `AddOrder_NoMatch` is the price level tree insertion. Specifically:
- Every unique price level that doesn't already exist causes a `new` call inside `std::map` to allocate a tree node.
- The benchmark creates a fresh price level for every order, so every single add triggers a heap allocation.

The secondary cost is order storage — the `unordered_map<OrderId, Order>` also triggers aligned allocations when it needs to grow or when a 64-byte Order is emplaced.

**IPC analysis:** 330B instructions / 120B cycles = **2.73 IPC**. On Apple M-series (capable of 4–5+ IPC when cache-hot), 2.73 IPC suggests moderate cache pressure but not a complete cache disaster. The allocator calls are the main culprit — they touch cold memory.

**The "always match" benchmark runs at 47M/s** because matching never inserts into the tree — it just removes from the front of an existing level. No allocation, pure linked-list traversal. That's 10x faster than add-with-insert.

---

## What to Work On Next

These are ordered by learning value and expected impact.

### 1. Pool allocator for OrderNodes

Every cancel and every partial fill currently calls `delete node`. Every new order calls `new OrderNode(...)`. The allocator overhead is visible in the profile — both the `new` inside `std::map` (for tree nodes) and the `new` inside `PriceLevel::add_order()`.

A simple fix: a fixed-size pool of `OrderNode` objects, allocated upfront as a contiguous array. `alloc()` returns the next free slot; `free()` returns it to the pool. No system malloc on the hot path.

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

Expected impact: removes the `operator new` samples from the profile for PriceLevel. The `std::map` tree node allocations would remain until the price level map itself is replaced.

### 2. Replace std::map with a flat hash map for price levels

`std::map` (red-black tree) allocates a heap node per price level and has poor cache locality — walking the tree pointer-chases through scattered memory. A flat hash map stores entries in a contiguous array. Cache-friendly, no per-entry allocation.

The tradeoff: `std::map` iterates in sorted order (useful for walking bids/asks to find the best price). A hash map doesn't. Solutions:
- Keep a separate sorted structure just for iteration (rarely needed on the hot path)
- Use `std::flat_map` (C++23) — sorted, contiguous, but O(n) insert in worst case
- Two structures: hash map for O(1) lookup, maintain `best_bid_`/`best_ask_` manually

This directly addresses what the profiler showed as the #1 bottleneck.

### 3. Fix get_bids() / get_asks() to avoid per-call allocation

`get_bids()` and `get_asks()` currently return `std::vector<PriceLevel*>` built fresh on each call. Options:
- Return a view/span into the internal data
- Pass in a pre-allocated output buffer
- Expose an iterator range instead of a concrete container

### 4. Symbol sharding across threads

Currently all symbols run on one matching thread. Sharding by symbol — routing AAPL orders to thread 0, GOOGL to thread 1, etc. — would scale throughput linearly with the number of independent symbols. Each shard gets its own SPSC queue and matching engine instance. No cross-shard synchronization needed since symbols are independent.

The interesting edge case: what do you do when a trader wants to leg into a cross-symbol spread? Real exchanges handle this with a separate spread book layer.

### 5. TCP order gateway

Everything currently runs in-process — agents submit orders by calling functions directly. Adding a TCP gateway would:
- Let you connect a real client (even a Python script sending raw bytes) to the simulator
- Separate submission latency from matching latency
- Teach you about the FIX protocol, or designing a binary wire format from scratch

A minimal gateway: `epoll`-based server (Linux) or `kqueue` (macOS), one connection per client, binary order struct over the wire, fill notifications streamed back.

### 6. Flamegraph on macOS

The `sample` tool gives a call tree as text. Xcode's Instruments can record the same data as a visual flamegraph:

```bash
xctrace record --template 'Time Profiler' --launch -- ./build/benchmarks \
    --benchmark_filter="AddOrder_NoMatch" --benchmark_min_time=8s
# then open the .trace file in Instruments.app
```

On Linux: `perf record -g ./benchmarks && perf script | stackcollapse-perf.pl | flamegraph.pl > flame.svg`

---

## Threading Model

Currently single-threaded matching with a simulated SPSC queue (the queue exists but producer and consumer run sequentially in the main loop). Full multi-threaded operation would look like:

```
Thread 0 (agents): agent.tick() → spsc_queue.push(order)
Thread 1 (engine): while (true) { spsc_queue.pop(order); engine.process(order); }
```

The queue supports true two-thread operation today — the agents just run synchronously for simplicity.

---

## Benchmark Notes

Numbers vary based on machine load and CPU frequency scaling. For stable readings:
- Pin to a single core: `taskset -c 0 ./benchmarks` (Linux) — macOS doesn't support `taskset`
- Build with `-O3 -march=native` (the Release preset does this)
- Run with no other CPU-intensive processes
- Use `--benchmark_min_time=3s` for tighter confidence intervals

The "AddOrder always match" benchmark (47M/s) is an upper bound — it fills from a pre-populated book with a single price level, so no tree insertion happens and the cache stays maximally hot. Real workloads sit somewhere between that and the "no match" case depending on order flow mix.
