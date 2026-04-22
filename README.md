# eXSimulator

A limit order book matching engine in C++20. Built to understand how exchange infrastructure actually works — not just at the concept level, but at the data structure and latency level. The design prioritizes correctness first, then measurable throughput.

Current throughput: **~5M orders/sec (add), ~10M/s (matching at scale), ~4.5M/s (cancel)**.

---

## Architecture

```
Trading Agents (producer thread)
    MarketMaker | Momentum | NoiseTrader
           │
           ▼
    SPSC Ring Buffer  ←── lock-free, cache-line padded
           │
           ▼
    Matching Engine   ←── routes by symbol, dispatches fills
           │
    ┌──────┼──────┐
    ▼      ▼      ▼
  AAPL   GOOGL   MSFT   (one OrderBook per symbol)
    │
    ├── Bids: map<Price, PriceLevel>
    └── Asks: map<Price, PriceLevel>
                │
           PriceLevel: doubly-linked list of OrderNodes
                       + unordered_map<OrderId, OrderNode*> for O(1) cancel
```

### Key Design Decisions

**Fixed-point prices** (`int64_t` in cents/ticks, not `double`). Floating-point is non-deterministic across platforms and accumulates rounding error. Every real exchange uses integer prices internally.

**64-byte Order struct** (one cache line). Hot fields — id, price, quantity, side, type — sit in the first 32 bytes. Cold fields — symbol, timestamp, trader id — come after. The matching engine only needs the hot fields; cold fields stay out of L1.

```
┌────────┬────────┬──────────┬──────┬──────────┬─────────────────────────┐
│order_id│ price  │ quantity │ side │   type   │  symbol / timestamp     │
│ 8 bytes│ 8 bytes│  4 bytes │ 1 b  │  1 byte  │  (cold, not on hot path)│
└────────┴────────┴──────────┴──────┴──────────┴─────────────────────────┘
 ◄──────────────── 32 bytes hot ──────────────────►
```

**Intrusive doubly-linked list in PriceLevel**. Each `OrderNode` stores `prev`/`next` pointers directly. O(1) front removal for matching, O(1) cancel via an `unordered_map<OrderId, OrderNode*>` index — no linear scan.

**SPSC queue between producer and consumer**. One thread generates orders, another consumes them. Single-producer single-consumer means no locks — just two atomic indices and cache-line padding to prevent false sharing.

**Fixed 8-byte Symbol**. `char[8]` stored inline. Comparing symbols is a single 64-bit integer compare, no heap allocation.

---

## Order Types

| Type     | Behavior                                                          |
| -------- | ----------------------------------------------------------------- |
| Limit    | Rest at price if not immediately matchable                        |
| Market   | Fill at best available price, cancel any remaining quantity       |
| IOC      | Fill whatever matches immediately, cancel the rest                |
| FOK      | Fill entirely or cancel entirely — no partial fills               |
| PostOnly | Reject if it would cross the spread (maker-only)                  |

---

## Performance

Measured on Apple M-series (10 cores), Release build (`-O3 -march=native`):

| Benchmark                      | Throughput | Notes                                                         |
| ------------------------------ | ---------- | ------------------------------------------------------------- |
| AddOrder (no match)            | ~4.5M/s    | Inserts resting order, worst case — new price level each time |
| AddOrder (always match)        | ~47M/s     | No insertion, just match and remove — cache stays hot         |
| AddOrder (same price level)    | ~7M/s      | Appends to existing level, no tree insert                     |
| CancelOrder                    | ~4.5M/s    | O(1) hashmap lookup — was 180K/s before the fix               |
| Matching throughput (100K)     | ~10M/s     | Full produce+match pipeline                                   |
| SPSC queue (single thread)     | ~104M/s    | Raw enqueue+dequeue cycle                                     |
| SPSC queue (producer/consumer) | ~11M/s     | Cross-thread, cache coherence overhead visible                |

The gap between "always match" (47M/s) and "no match" (4.5M/s) is the cost of a new price level insertion — heap alloc plus red-black tree rebalancing. That's the next thing to address.

---

## Build

Requires CMake 3.20+, C++20 compiler (clang 14+ or gcc 12+).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

With benchmarks (requires Google Benchmark):

```bash
brew install google-benchmark catch2   # macOS
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build --target benchmarks
```

## Run

```bash
# Simulator: 2 symbols, 10 agents, 5000 ticks
./build/exchange_sim --symbols 2 --agents 10 --ticks 5000

# Unit tests (37 cases)
./build/test_order_book

# Benchmarks (pin to a single core for stable readings)
taskset -c 0 ./build/benchmarks        # Linux
./build/benchmarks                     # macOS (no taskset)
```

---

## Project Structure

```
include/
├── core/       # Order, Trade, Price, Quantity types
├── engine/     # MatchingEngine, OrderBook, PriceLevel
├── agents/     # Trading agent interfaces
├── replay/     # Event journal (binary log)
└── utils/      # SPSC queue, latency histogram, timing

src/            # Implementations
tests/
├── unit/               # Catch2 unit tests
├── benchmarks/         # Google Benchmark suite
└── test_order_book.cpp # Integration tests (37 cases)

docs/
└── DESIGN.md           # Architecture deep-dive and profiling results
```

---

## References

- [How to Build a Fast Limit Order Book](https://web.archive.org/web/20110219163448/http://howtohft.wordpress.com/2011/02/15/how-to-build-a-fast-limit-order-book/) — canonical writeup on data structure choices
- [Trading at Light Speed](https://www.youtube.com/watch?v=NH1Tta7purM) — David Gross, Meeting C++ 2022
- [Lock-Free Programming](https://preshing.com/20120612/an-introduction-to-lock-free-programming/) — Preshing
- [Erik Rigtorp's SPSCQueue](https://github.com/rigtorp/SPSCQueue) — reference for the queue implementation
- [What Every Programmer Should Know About Memory](https://people.freebsd.org/~lstewart/articles/cpumemory.pdf) — Drepper
