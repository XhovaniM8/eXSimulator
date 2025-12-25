# Exchange Simulator

A low-latency, deterministic limit-order-book exchange with matching engine, simulated trading agents, replay/backtest harness, and analytics dashboard.

## What This Is

A high-performance exchange simulator that processes orders, matches trades, and reports throughput/latency metrics. Built to demonstrate:

- Correctness under concurrency (lock-free data structures, deterministic replay)
- High-performance C++ (cache-friendly layouts, SPSC queues, zero-copy where possible)  
- Systems design (event sourcing, symbol sharding, backpressure)
- Measurable results (orders/sec, p50/p99 latency histograms, memory footprint)
- Full-stack integration (WebSocket market data, REST control plane, real-time dashboard)

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              Control Plane (REST)                           │
│                        start/stop, config, scenario select                  │
└─────────────────────────────────────────────────────────────────────────────┘
                                       │
┌─────────────────────────────────────────────────────────────────────────────┐
│                                Gateway Layer                                │
│                    FIX/Binary protocol → Internal messages                  │
└─────────────────────────────────────────────────────────────────────────────┘
                                       │
                    ┌──────────────────┼──────────────────┐
                    ▼                  ▼                  ▼
           ┌──────────────┐   ┌──────────────┐   ┌──────────────┐
           │  OrderBook   │   │  OrderBook   │   │  OrderBook   │
           │    AAPL      │   │    GOOGL     │   │    MSFT      │
           │  (sharded)   │   │  (sharded)   │   │  (sharded)   │
           └──────────────┘   └──────────────┘   └──────────────┘
                    │                  │                  │
                    └──────────────────┼──────────────────┘
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                              Event Journal                                  │
│            append-only log (binary), enables deterministic replay           │
└─────────────────────────────────────────────────────────────────────────────┘
                                       │
                    ┌──────────────────┼──────────────────┐
                    ▼                  ▼                  ▼
           ┌──────────────┐   ┌──────────────┐   ┌──────────────┐
           │  Market Data │   │   Metrics    │   │   Trades     │
           │   Publisher  │   │   Collector  │   │   Reporter   │
           └──────────────┘   └──────────────┘   └──────────────┘
                    │                  │                  │
                    └──────────────────┼──────────────────┘
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           WebSocket/REST API                                │
│              L2 book, trades tape, latency histograms, PnL                  │
└─────────────────────────────────────────────────────────────────────────────┘
                                       │
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           Web Dashboard (React)                             │
│         order book viz, trades feed, agent controls, perf charts            │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Repository Structure

```
exchange-simulator/
├── src/
│   ├── core/           # Fundamental types: Order, Trade, Price, Quantity
│   ├── engine/         # Matching engine, OrderBook, PriceLevels
│   ├── agents/         # Trading strategies: MarketMaker, Momentum, Noise
│   ├── replay/         # Event journal, deterministic replay harness
│   ├── network/        # Gateway, WebSocket publisher, REST server
│   └── utils/          # SPSC queue, memory pool, timing, histograms
├── include/            # Public headers (mirrors src/)
├── tests/
│   ├── unit/           # Catch2 unit tests
│   ├── integration/    # Multi-component tests
│   └── benchmarks/     # Google Benchmark, latency measurement
├── dashboard/          # React web UI
│   ├── src/
│   └── public/
├── scripts/            # Build, benchmark, flamegraph generation
├── docs/               # Design docs, API spec, performance reports
├── data/               # Sample data, replay logs
└── cmake/              # CMake modules
```

## Implementation

### Core Engine

| Component | Description | Key Challenges |
|-----------|-------------|----------------|
| `OrderBook` | Price-time priority LOB with bid/ask sides | Cache-friendly level storage, O(1) best price |
| `MatchingEngine` | Processes orders, emits fills | Partial fills, self-trade prevention |
| `PriceLevel` | FIFO queue per price, intrusive linked list | O(1) add/cancel, memory locality |
| `SPSCQueue` | Lock-free inter-thread communication | Memory ordering, false sharing avoidance |
| `MemoryPool` | Pre-allocated order storage | Zero allocation on hot path |

### Replay & Persistence

| Component | Description | Key Challenges |
|-----------|-------------|----------------|
| `EventJournal` | Append-only binary log with CRC | Zero-copy writes, mmap for replay |
| `ReplayHarness` | Deterministic re-execution | Byte-for-byte reproducibility |

### Trading Agents

| Component | Description | Key Challenges |
|-----------|-------------|----------------|
| `MarketMaker` | Two-sided quotes, inventory skew | Position limits, spread calculation |
| `Momentum` | Trend-following signals | Lookback window, threshold tuning |
| `NoiseTrader` | Random order flow for liquidity | Configurable order mix |

### Network & Dashboard

| Component | Description | Key Challenges |
|-----------|-------------|----------------|
| `WebSocket Server` | Publishes L2 book, BBO, trades | Backpressure, binary framing |
| `REST API` | Control plane: start/stop/config | Validation, state management |
| `React Dashboard` | Real-time order book, latency charts | Efficient re-renders |

### Benchmarking

| Component | Description | Key Challenges |
|-----------|-------------|----------------|
| `Timing` | rdtsc cycle-accurate measurement | Calibration, TSC invariance |
| `Histogram` | HDR histogram for percentiles | Memory-efficient bucketing |
| `LoadGenerator` | Synthetic order flow | Realistic patterns |

## Goals

See [ROADMAP.md](ROADMAP.md) for detailed phased plan.

### Phase 1-3 Targets (Months 1-2)
- **Throughput**: 10K-50K orders/sec
- **Correctness**: All unit tests pass, edge cases covered
- **Tooling**: CI green, basic benchmarks running

### Phase 4-6 Targets (Months 2-4)
- **Throughput**: 100K-500K orders/sec  
- **p50 Latency**: <50µs
- **Architecture**: Memory pool, cache-aligned structs, SPSC queue

### Final Targets (Months 4-6)
- **Throughput**: 500K+ orders/sec (stretch: 1M+)
- **p50 Latency**: <10µs (stretch: <1µs)
- **p99 Latency**: <100µs (stretch: <10µs)
- **Deterministic Replay**: 100% reproducible

### Always
- **FIFO Matching**: Price-time priority enforced
- **Observability**: Latency histograms, throughput metrics, flamegraphs

## Build

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

## Run

```bash
# Run matching engine with sample load
./bin/exchange_sim --symbols 10 --agents 100

# Run benchmarks
./bin/benchmarks --benchmark_format=json > results.json

# Generate flamegraph
./scripts/flamegraph.sh ./bin/exchange_sim

# Start dashboard (separate terminal)
cd dashboard && npm run dev
```

## References

- [How to Build a Fast Limit Order Book](https://web.archive.org/web/20110219163448/http://howtohft.wordpress.com/2011/02/15/how-to-build-a-fast-limit-order-book/)
- [Liquibook](https://github.com/enewhuis/liquibook) - Open source C++ matching engine
- [Trading at Light Speed](https://www.youtube.com/watch?v=NH1Tta7purM) - David Gross, Meeting C++ 2022
- [Lock-Free Programming](https://preshing.com/20120612/an-introduction-to-lock-free-programming/)
- [Erik Rigtorp's SPSCQueue](https://github.com/rigtorp/SPSCQueue)
