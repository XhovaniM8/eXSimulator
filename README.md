# Exchange Simulator

A high-performance limit order book matching engine in C++20. Processes **7.53M ops/sec** on live market data with 132.9ns average latency on commodity x86 hardware.

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
- Live market data recorder + binary replay pipeline (Kraken WebSocket)

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
| **Live market data (Kraken XBT/USD)** | **7.53M/s** |
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

### Live market data validation

Validated against 5 minutes of live Kraken XBT/USD order book data — real price
clustering, burst cancellations, and realistic market microstructure:

| Metric | Result |
|--------|--------|
| Commands replayed | 15,542 real market events |
| Throughput | 7.53M ops/sec |
| Latency/op | 132.9ns average |
| Real Kraken market rate | ~52 ops/sec |
| Engine headroom | ~136,000x faster than market |

> Data captured via `scripts/record_kraken.py`, parsed with `scripts/parse_feed.py`,
> replayed with `build/bin/replay_kraken`. 3.7x faster than documented production
> engines (~2M ops/sec on comparable hardware).

**Compared to similar open-source projects:**

| Project | Throughput |
|---------|-----------|
| timothewt/OrderBook | 600K/s |
| brprojects/Limit-Order-Book | 1.4M/s |
| Liquibook (OCI) | 2.5M/s |
| **eXSimulator (this project)** | **3.56M–7.53M/s (real data)** |
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

# Live market data benchmark (Kraken XBT/USD)
source scripts/venv/bin/activate
python3 scripts/record_kraken.py --duration 60 --output data/kraken.jsonl
python3 scripts/parse_feed.py --input data/kraken.jsonl --output data/kraken.bin
./bin/replay_kraken data/kraken.bin
```

## Project Structure

```
src/
├── core/       # Order, Trade, Price, Quantity types
├── engine/     # MatchingEngine, OrderBook, PriceLevel
├── agents/     # MarketMaker, Momentum, Noise traders
├── replay/     # Event journal, replay harness (skeleton)
├── tools/      # replay_kraken — live market data benchmark
└── utils/      # SPSC queue, timing, histograms

scripts/
├── record_kraken.py   # WebSocket recorder for Kraken order book
├── parse_feed.py      # Converts JSONL feed to binary replay format
├── benchmark.sh       # Benchmark runner
└── flamegraph.sh      # Flame graph generator

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
- [Benchmark Dataset for Mid-Price Forecasting of LOB Data](https://arxiv.org/abs/1705.03233) - Ntakaris et al. 2017
