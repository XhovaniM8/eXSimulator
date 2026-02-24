# Exchange Simulator

A high-performance limit order book matching engine in C++20. Processes **5.8M orders/sec** (9.7M at scale) with sub-microsecond matching on commodity x86 hardware.

## Current Status

Phase 3 complete (benchmarking). Phase 4 (data structure optimization) next. See [ROADMAP.md](ROADMAP.md) for details.

**What works:**
- Full matching engine with price-time priority (FIFO)
- Order types: Limit, Market, IOC, FOK, PostOnly
- Cancel and replace orders
- Self-trade prevention
- Lock-free SPSC queue for order ingestion (610M ops/sec)
- Multi-symbol support with sharding
- Trading agents: MarketMaker, Momentum, Noise
- Latency histograms
- Google Benchmark micro-benchmarks (37/37 tests passing)

## Performance

Measured on AMD Ryzen (8-core, 4.7GHz), Release build, pinned to single core:

| Benchmark | Result |
|-----------|--------|
| AddOrder (no match) | 5.8M/s |
| AddOrder (always match) | 25.6M/s |
| Matching throughput (100K orders) | 9.7M/s |
| Batch processing (10K orders) | 6.0M/s |
| SPSC queue throughput | 610M/s |
| Cancel order | 180K/s |

> Cancel is the known bottleneck — O(n) linear scan in PriceLevel. Targeted for Phase 4 optimization.

**Compared to similar open-source projects:**

| Project | Throughput |
|---------|-----------|
| timothewt/OrderBook | 600K/s |
| brprojects/Limit-Order-Book | 1.4M/s |
| Liquibook (OCI) | 2.5M/s |
| **eXSimulator (this project)** | **5.8M–9.7M/s** |
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
- `PriceLevel` uses intrusive doubly-linked list (O(1) front removal)
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
./exchange_sim --symbols 2 --agents 10 --ticks 5000

# Order book tests (37/37 passing)
./test_order_book

# Feature tests
./feature_test

# Benchmarks (pin to core for stable results)
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
- [What Every Programmer Should Know About Memory](https://people.freebsd.org/~lstewart/articles/cpumemory.pdf)/rigtorp/SPSCQueue)
