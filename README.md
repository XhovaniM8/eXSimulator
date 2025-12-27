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

---

## Future Additions

> **Prerequisites**: Core matching engine stable, throughput ≥100K orders/sec, all unit tests passing, basic UI functional.

These extensions move the simulator from "toy project" to something that models real market dynamics. Most open-source exchange simulators ignore these - implementing them is what makes this project unique.

### Realistic Market Microstructure

| Feature | Description | Why It Matters |
|---------|-------------|----------------|
| **Adverse Selection Model** | Informed vs. uninformed trader classification; market makers get "picked off" by informed flow | Models the actual P&L dynamics of market making - why spreads exist |
| **Queue Position Uncertainty** | Simulate not knowing your exact queue position; probabilistic fill modeling | Real traders don't know where they are in the queue - critical for strategy backtesting |
| **Latency Arbitrage Scenarios** | Configurable per-agent network delays; "slow" vs. "fast" participant dynamics | Demonstrates understanding of the latency arms race |

### Advanced Order Types

| Order Type | Behavior | Implementation Notes |
|------------|----------|----------------------|
| **IOC** (Immediate or Cancel) | Fill what's available, cancel remainder | Modify matching loop to not rest unfilled qty |
| **FOK** (Fill or Kill) | Fill entirely or reject | Pre-check available liquidity before matching |
| **Iceberg / Hidden** | Display qty < total qty; replenish on fill | Separate display_qty field; re-add to queue on partial |
| **Stop / Stop-Limit** | Trigger on price threshold | Maintain stop book; scan on each trade |
| **Pegged Orders** | Price tracks BBO with offset | Re-price on book updates; cancel/replace internally |

### Auction Mechanisms

Most simulators run continuous matching only. Real exchanges use auctions for critical moments:

| Auction Type | When Used | Key Mechanics |
|--------------|-----------|---------------|
| **Opening Auction** | Market open | Collect orders, calculate uncrossing price, single multilateral match |
| **Closing Auction** | Market close | Same as opening; closing price is reference for derivatives/ETFs |
| **Volatility Auction** | Circuit breaker triggered | Pause continuous trading, run call auction to find new equilibrium |
| **Intraday Auction** | Scheduled or on-demand | Liquidity aggregation for illiquid names |

Implementation requires: auction order book (no immediate matching), uncrossing price algorithm (maximize volume, minimize imbalance), state machine for trading phases.

### Multi-Venue & Smart Order Routing

Simulate a fragmented market with multiple venues:

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  Venue A    │     │  Venue B    │     │  Venue C    │
│  Low fees   │     │  Deep book  │     │  Fast       │
│  Slow       │     │  High fees  │     │  Thin book  │
└─────────────┘     └─────────────┘     └─────────────┘
       │                  │                   │
       └──────────────────┼───────────────────┘
                          ▼
                 ┌─────────────────┐
                 │  Smart Order    │
                 │  Router (SOR)   │
                 └─────────────────┘
```

| SOR Strategy | Logic | Demonstrates |
|--------------|-------|--------------|
| **Fee Optimization** | Route to minimize maker/taker fees | Understanding of exchange economics |
| **Latency-Aware** | Prefer faster venues for time-sensitive orders | Real HFT concern |
| **Liquidity Seeking** | Sweep multiple venues, size-weighted | Basic SOR functionality |
| **Anti-Gaming** | Randomize routing to avoid detection | Adversarial thinking |

### Realistic Fill Simulation (Backtesting Mode)

Replace naive "order = fill" with realistic execution modeling:

| Model | Description | Parameters |
|-------|-------------|------------|
| **Queue Position Model** | Track simulated queue position; fill only when volume trades through | Join position, cancel rates ahead |
| **Fill Probability** | Probabilistic fills based on order size vs. level depth | Calibrated from real data |
| **Market Impact** | Your order moves the price; larger orders = more slippage | Permanent vs. temporary impact coefficients |
| **Partial Fill Dynamics** | Model realistic partial fill sequences | Fill rate decay over time |

### Risk Engine (Pre-Trade)

Real exchanges reject bad orders before they hit the book:

| Check | Description | Action |
|-------|-------------|--------|
| **Fat Finger** | Price > X% away from reference | Reject |
| **Position Limits** | Agent position would exceed max | Reject |
| **Order Size Limits** | Single order > max notional | Reject |
| **Rate Limits** | Orders/sec exceeds threshold | Reject or throttle |
| **Credit Check** | Notional exceeds available margin | Reject |

### FPGA Acceleration (Long-Term)

> **Prerequisites**: Software implementation stable, latency profiled, hot path identified.

Design the engine now with hardware offload in mind:

| Component | Software Baseline | FPGA Target | Notes |
|-----------|-------------------|-------------|-------|
| **Order Parsing** | ~500ns | <100ns | Fixed-format binary protocol |
| **Book Lookup** | ~200ns | <50ns | Content-addressable memory |
| **Match + Fill** | ~1µs | <200ns | Pipeline the comparison tree |
| **Market Data Fan-out** | ~5µs | <500ns | Multicast in fabric |

**Acceleration Boundary**: Define a clean interface between "control plane" (stays in software) and "data plane" (moves to FPGA):

```
┌─────────────────────────────────────────┐
│           Software (Control)            │
│  - Agent logic, risk parameters         │
│  - Symbology, reference data            │
│  - Slow-path order types (stops, etc.)  │
└─────────────────────────────────────────┘
                    │
          ══════════════════════  ← Acceleration Boundary
                    │
┌─────────────────────────────────────────┐
│            FPGA (Data Plane)            │
│  - Order ingress parsing                │
│  - Book storage + matching              │
│  - Market data egress                   │
└─────────────────────────────────────────┘
```

Target boards: Alveo U50/U250, or Intel Stratix 10. Use a framework like Vitis HLS or write raw RTL for tightest control.

---

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
