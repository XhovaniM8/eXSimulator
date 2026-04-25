# Learning Goals & What's Left to Build

This file tracks what's been learned and what's worth exploring next. No timeline, no phases — just areas of systems and HFT knowledge organized by what you'd gain from working on them.

---

## What's Been Built and What Was Learned

### Limit Order Book Core

Built a full price-time priority matching engine with all standard order types (Limit, Market, IOC, FOK, PostOnly), cancel, replace, and self-trade prevention.

**What you learned:** How an order book is actually structured. Why price-time priority means per-level FIFO queues, not a global sort. Why best bid/ask is cached rather than computed. How cancel semantics work (you cancel by order ID, not by price).

### Fixed-Point Arithmetic

All prices are `int64_t` in integer ticks. No `double`, no `float`.

**What you learned:** Why floating-point is dangerous in financial systems — non-determinism across compilers and platforms, accumulating rounding error. Fixed-point is boring and correct. This is what real exchanges do.

### Cache-Aligned Order Struct

`Order` is exactly 64 bytes — one cache line. Hot fields (id, price, qty, side, type) in the first 32 bytes. Cold fields (symbol, timestamp, trader id) in the second 32 bytes.

**What you learned:** Cache lines are the unit of memory transfer. If the fields the matching engine needs on the hot path sit in the first cache line, the cold fields never need to be loaded. Hot/cold separation is a real technique used in HFT.

### Lock-Free SPSC Queue

Single-producer single-consumer ring buffer with cache-line padded indices. No mutexes.

**What you learned:** Why SPSC is special — you only need acquire/release semantics, not CAS loops. Why false sharing kills performance (the producer and consumer fight over the same cache line if indices aren't padded). How to think about memory ordering without going insane.

### O(1) Cancel via Hashmap Index

PriceLevel went from O(n) linear scan (vector) to O(1) hashmap lookup. Improvement: 180K/s → ~4.5M/s (25x).

**What you learned:** Profiling before optimizing — the bottleneck was obvious once measured. How to maintain a parallel index structure (the linked list for FIFO order, the hashmap for random access) without breaking consistency.

### Benchmarking with Google Benchmark

Micro-benchmarks for add, cancel, match, and queue operations. Profiling with macOS `sample`.

**What you learned:** How to write meaningful benchmarks (fixture setup, preventing dead code elimination via `DoNotOptimize`). What the numbers actually mean — "always match" is 10x faster than "no match" because one allocates and one doesn't. How to read a CPU sample call tree.

---

## Next Steps

### Memory Allocator: Pool Allocator for OrderNodes

**The problem:** Every `new OrderNode(...)` calls the system allocator. The profiler shows `operator new` accounting for ~7% of samples in the hot path. The allocator touches cold memory (its internal free lists, OS pages).

**What you'd learn:** How pool allocators work. Why HFT code often pre-allocates everything at startup. The difference between throughput-optimized and latency-optimized allocation strategies. You can benchmark before/after easily.

**How hard:** ~100 lines of code. Low risk — PriceLevel is self-contained.

### Data Structure: Replace std::map with a Flat Hash Map

**The problem:** `std::map<Price, PriceLevel>` is a red-black tree. Each price level insertion allocates a tree node. Tree traversal is pointer-chasing through heap-scattered nodes. The profiler puts ~60% of `AddOrder_NoMatch` samples here.

**What you'd learn:** Why cache locality matters for tree-structured data. How open-addressing hash maps (flat_hash_map) compare to chained hash maps (unordered_map). The tradeoff: trees give sorted iteration, hash maps don't — and how to deal with that.

**How hard:** Medium. Requires careful handling of the "iterate in price order" case (for BBO updates and order book display).

### Concurrency: True Multi-Threaded Producer-Consumer

**The problem:** Agents and engine currently run synchronously on one thread. The SPSC queue is implemented and works, but the producer and consumer aren't on separate threads.

**What you'd learn:** How to pin threads to cores. How SPSC latency changes under real cross-thread communication vs. single-thread simulation. What NUMA awareness means and whether it matters here.

**How hard:** Low — the infrastructure is already there. It's mostly a `std::thread` + `pthread_setaffinity_np` exercise.

### Concurrency: Symbol Sharding

**The problem:** All symbols go through one matching thread.

**What you'd learn:** How to partition work by key (symbol) to scale horizontally. Why sharding works (symbols are independent — no AAPL order affects a GOOGL order). The interesting exception: spread orders that cross symbols.

**How hard:** Medium. Requires routing logic and one matching thread per shard.

### Realism: TCP Order Gateway

**The problem:** Everything runs in-process. There's no way to connect an external client.

**What you'd learn:** How exchange gateways actually work. How to design a binary wire protocol (or a subset of FIX). How network latency relates to matching latency. `epoll`/`kqueue` for low-latency I/O.

**How hard:** Medium-high. A minimal version (binary struct over TCP, no FIX) is maybe 500 lines. FIX protocol is a rabbit hole.

### Realism: Better Agent Behavior

**The problem:** MarketMaker, Momentum, and Noise traders are very simple. The book they generate doesn't behave like a real market.

**What you'd learn:** Basic market microstructure — how spreads emerge, why momentum strategies create trends, what informed vs. uninformed order flow looks like. How to model mean-reverting prices. PnL tracking and position limits.

**How hard:** Low-medium. Mostly math and calibration, not systems code.

### Observability: Flamegraph

**The problem:** The `sample` call tree is hard to read at a glance. A flamegraph makes hot paths obvious visually.

**What you'd learn:** How to use Instruments (macOS) or `perf` + FlameGraph scripts (Linux) to generate a flamegraph. How to interpret one — wide bars are hot, tall stacks are deep call chains.

**How hard:** Very low. Just a tooling exercise.

### Replay: Deterministic Event Journal

**The problem:** The event journal skeleton exists but replay isn't implemented. You can't reproduce a simulation exactly.

**What you'd learn:** Event sourcing — how to record all inputs, replay them, and get the same output. Why this matters for debugging (reproduce the exact state at crash time) and backtesting (run historical data through the engine).

**How hard:** Medium. The journal format needs care — binary, versioned, order-preserving.

---

## Current Performance Snapshot

Measured on Apple M-series (10 cores), Release build (`-O3 -march=native`):

| Benchmark | Throughput |
|-----------|-----------|
| AddOrder (no match) | ~4.5M/s |
| AddOrder (always match) | ~47M/s |
| AddOrder (same price level) | ~7M/s |
| CancelOrder (PriceLevel) | ~4.5M/s |
| CancelOrder (MatchingEngine) | ~7M/s |
| Matching throughput (100K) | ~10M/s |
| SPSC queue (single thread) | ~104M/s |
| SPSC queue (producer-consumer) | ~11M/s |

The 10x gap between "always match" and "no match" is the cost of `std::map` tree insertion + heap allocation. That's the next measurable thing to attack.
