# Exchange Simulator

A high-performance limit order book matching engine in C++20. Processes **7.7M orders/sec** matching throughput with sub-microsecond cancel latency on commodity x86 hardware.

## Current Status

Phase 4 in progress (data structure optimization). Phase 5 (memory & cache optimization) next. See [ROADMAP.md](ROADMAP.md) for details.

**What works:**
- Full matching engine with price-time priority (FIFO)
- Order types: Limit, Market, IOC, FOK, PostOnly
- Cancel and replace orders
- Self-trade prevention
- Lock-free SPSC queue for order ingestion (596M ops/sec)
- Multi-symbol support with sharding
- Trading agents: MarketMaker, Momentum, Noise
- Latency histograms
- Google Benchmark micro-benchmarks (37/37 tests passing)

## Performance

Measured on **Intel Core i7-9700F @ 3.00GHz** (8-core, no HT), Release build, pinned to single core (`taskset -c 0`), performance governor enabled:

| Benchmark | Result |
|-----------|--------|
| AddOrder (no match) | 3.56M/s |
| AddOrder (always match) | 30.5M/s |
| AddOrder (same price level) | 3.68M/s |
| Cancel order | 5.93M/s |
| Matching throughput (100K orders) | 7.70M/s |
| Batch processing (10K orders) | 4.61M/s |
| SubmitOrder single symbol | 3.33M/s |
| SubmitOrder multi-symbol | 2.06M/s |
| SPSC queue (single thread) | 596M/s |

### Cache hierarchy
```
L1 Data:    32 KiB × 8
L2 Unified: 256 KiB × 8
L3 Unified: 12 MiB shared
```

### Phase 4 optimization results

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| Cancel order | 180K/s | 5.93M/s | **33x** |
| Simulator ticks/sec | 2,903 | 7,726 | **2.66x** |
| cancel_order CPU% | 72% | 10% | **7.2x** |

**What changed:**
- Replaced `std::vector<OrderNode*>` linear scan with `unordered_map<OrderId, OrderNode*>` in `PriceLevel` → O(1) cancel lookup
- Swapped `bid_levels_`/`ask_levels_` from `unordered_map` to `std::map` → O(1) best bid/ask via `rbegin()`/`begin()` instead of full scan on every cancel

**Compared to similar open-source projects:**

| Project | Throughput |
|---------|-----------|
| timothewt/OrderBook | 600K/s |
| brprojects/Limit-Order-Book | 1.4M/s |
| Liquibook (OCI) | 2.5M/s |
| **eXSimulator (this project)** | **3.56M–30.5M/s** |
| aanrv/Order-Book | 10.8M/s |

## Architecture

```
              ┌──────────────────┬──────────────────┐
              ▼                  ▼                  ▼
     ┌──────────────┐   ┌──────────────┐   ┌──────────────┐
     │  OrderBook   │   │  OrderBook   │   │  OrderBook   │
     │    AAPL      │   │    GOOGL     │   │    MSFT      │
     └──────────────┘   └──────────────┘   └──────────────┘
              │                  │                  │
              └──────────────────┼──────────────────┘
                                 ▼
                        Matching Engine
                                 │
                        ┌────────┴────────┐
                        │   SPSC Queue    │
                        │  (lock-free)    │
                        └─────────────────┘
```

**Key design decisions:**
- `Price` is `int64_t` (fixed-point, no floating point errors)
- `Order` struct is exactly 64 bytes (one CPU cache line)
- `Symbol` is fixed 8-byte char array (no heap allocation)
- `PriceLevel` uses intrusive doubly-linked list + `unordered_map` index for O(1) cancel
- `bid_levels_`/`ask_levels_` use `std::map` for O(1) best bid/ask via iterator endpoints
- Best bid/ask cached for O(1) BBO access

## Build

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### With Benchmarks

```bash
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON ..
make -j$(nproc)
```

Install Google Benchmark from source for accurate results:
```bash
git clone https://github.com/google/benchmark.git /tmp/benchmark
cmake -DCMAKE_BUILD_TYPE=Release -DBENCHMARK_DOWNLOAD_DEPENDENCIES=ON -B /tmp/benchmark/build -S /tmp/benchmark
cmake --build /tmp/benchmark/build -j$(nproc)
sudo cmake --install /tmp/benchmark/build
```

## Run

```bash
# Main simulator
./exchange_sim

# Order book tests (37/37 passing)
./test_order_book

# Feature tests
./feature_test

# Benchmarks (pin to core for stable results)
sudo cpupower frequency-set -g performance
taskset -c 0 ./benchmarks
```

## Project Structure

```
src/
├── core/       # Order, Trade, Price, Quantity types
├── engine/     # MatchingEngine, OrderBook, PriceLevel
├── agents/     # MarketMaker, Momentum, Noise traders
├── replay/     # Event journal, replay harness (skeleton)
└── utils/      # SPSC queue, timing, histograms

tests/
├── unit/               # Catch2 unit tests
├── benchmarks/         # Google Benchmark suite
└── test_order_book.cpp # Integration tests (37 cases)
```

## References

- [How to Build a Fast Limit Order Book](https://web.archive.org/web/20110219163448/http://howtohft.wordpress.com/2011/02/15/how-to-build-a-fast-limit-order-book/)
- [Trading at Light Speed](https://www.youtube.com/watch?v=NH1Tta7purM) - David Gross, Meeting C++ 2022
- [Erik Rigtorp's SPSCQueue](https://github.com/rigtorp/SPSCQueue)
- [What Every Programmer Should Know About Memory](https://people.freebsd.org/~lstewart/articles/cpumemory.pdf)
