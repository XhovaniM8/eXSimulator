# Exchange Simulator Roadmap

## Current Status: Phase 3 In Progress

**Last Updated**: February 2026

### Performance Achieved
| Metric | Result | Original Target | Status |
|--------|--------|-----------------|--------|
| Throughput | **3.3M orders/sec** | 10K orders/sec | 330x target |
| p50 Latency | **32us** | <100us | Done |
| p99 Latency | **32us** | <1ms | Done |
| Trades/sec | **8,700** | - | Done |

### What's Working
- Full matching engine with price-time priority
- All order types: Limit, Market, IOC, FOK, PostOnly
- Self-trade prevention (basic)
- SPSC lock-free queue for order ingestion
- Cache-line aligned Order struct (64 bytes)
- Multi-symbol support with sharding
- Trading agents: MarketMaker, Momentum, Noise
- Latency histograms (HDR)
- Google Benchmark micro-benchmarks
- CI/CD pipeline

---

## Phase Overview

### Phase 1: Foundation [Complete]
**Duration**: 1 week (vs 2 weeks planned)

- [x] Order struct with fixed-size fields
- [x] Symbol type (8-byte fixed)
- [x] Price/Quantity types
- [x] Side, OrderType, TimeInForce enums
- [x] Basic OrderBook skeleton
- [x] CMake build system
- [x] CI pipeline (GitHub Actions)

### Phase 2: Matching Engine [Complete]
**Duration**: 1 week (vs 2 weeks planned)

- [x] Price-time priority (FIFO within price level)
- [x] Limit order matching
- [x] Market orders
- [x] IOC (Immediate or Cancel)
- [x] FOK (Fill or Kill)
- [x] PostOnly (maker-only, reject if would cross)
- [x] Cancel order
- [x] Replace order (cancel + new)
- [x] Self-trade prevention (order ID proximity)
- [x] Unit tests for all order types
- [x] Feature test executable

**Known Issues (P2)**:
- Self-trade prevention uses order ID proximity as proxy for trader ID
- `get_bids()`/`get_asks()` allocate on each call (optimize in Phase 4)

### Phase 3: Benchmarking [In Progress]
**Duration**: 1 week

- [x] Baseline measurement: **3.3M orders/sec**
- [x] Latency histogram: **32us p50/p99**
- [x] Google Benchmark integration (order book, matching engine, queue benchmarks)
- [ ] Google Benchmark in CI
- [ ] Profiling with perf/flamegraph
- [ ] Identify hotspots for Phase 4
- [ ] Document baseline in performance report

**Run benchmarks**:
```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON ..
make -j$(nproc)
./benchmarks
```

### Phase 4: Data Structure Optimization [Pending]
**Duration**: 2 weeks
**Target**: 5M+ orders/sec, <20us p50

- [ ] Replace `std::unordered_map` with flat hashmap for price levels
- [ ] Intrusive linked list for order queue (avoid allocations)
- [ ] Pool allocator for Order objects
- [ ] Batch order processing
- [ ] Profile cache misses with `perf stat`

### Phase 5: Memory & Cache Optimization [Pending]
**Duration**: 1-2 weeks
**Target**: 10M+ orders/sec, <10us p50

- [ ] Hot/cold data separation in Order struct
- [ ] Prefetch hints for likely code paths
- [ ] Branch prediction hints (`[[likely]]`/`[[unlikely]]`)
- [ ] Measure cache hit rates

### Phase 6: Lock-Free & Concurrency [Pending]
**Duration**: 1-2 weeks
**Target**: Maintain throughput under contention

- [x] SPSC queue already implemented
- [ ] Symbol sharding across threads
- [ ] Lock-free order ID generation
- [ ] Thread pinning for latency stability
- [ ] NUMA-aware allocation (if applicable)

### Phase 7: Trading Agents [Pending]
**Duration**: 1 week

- [x] MarketMaker (two-sided quotes)
- [x] Momentum (trend following)
- [x] NoiseTrader (random flow)
- [ ] Agent configuration via JSON
- [ ] Realistic PnL tracking
- [ ] Position/risk limits per agent

### Phase 8: Replay & Backtesting [Pending]
**Duration**: 1 week

- [x] Event journal (binary log skeleton)
- [x] Replay harness skeleton
- [ ] Deterministic replay (byte-for-byte)
- [ ] Backtesting mode with fill simulation
- [ ] Replay from historical data files

### Phase 9: Dashboard & API [Pending]
**Duration**: 2-3 weeks

- [ ] WebSocket market data publisher
- [ ] REST control plane (start/stop/config)
- [ ] React dashboard
  - [ ] Real-time order book visualization
  - [ ] Trades feed
  - [ ] Latency charts
  - [ ] Agent controls

### Phase 10: Polish & Documentation [Pending]
**Duration**: 1 week

- [ ] Code cleanup and documentation
- [ ] Design doc explaining tradeoffs
- [ ] Performance write-up with charts
- [ ] README with impressive numbers
- [ ] Video demo

---

## Timeline Summary

| Phase | Planned | Actual | Status |
|-------|---------|--------|--------|
| 1. Foundation | 2 weeks | 1 week | Complete |
| 2. Matching Engine | 2 weeks | 1 week | Complete |
| 3. Benchmarking | 1 week | - | In Progress |
| 4. Data Structures | 2 weeks | - | Pending |
| 5. Memory & Cache | 2 weeks | - | Pending |
| 6. Lock-Free | 2 weeks | - | Pending |
| 7. Agents | 1 week | - | Pending |
| 8. Replay | 1 week | - | Pending |
| 9. Dashboard | 3 weeks | - | Pending |
| 10. Polish | 1 week | - | Pending |

---

## Performance Targets by Phase

| Phase | Throughput | p50 Latency | p99 Latency |
|-------|------------|-------------|-------------|
| 2 (Baseline) | 10K/sec | <100us | <1ms |
| **2 (Actual)** | **3.3M/sec** | **32us** | **32us** |
| 3 (Measured) | 3M+/sec | <50us | <100us |
| 4 (Optimized) | 5M+/sec | <20us | <50us |
| 5 (Cache) | 10M+/sec | <10us | <20us |
| 6 (Final) | 10M+/sec | <5us | <10us |

---

## FAQ

### Why not start with lock-free queues?
Lock-free code is hard to debug. Get correctness first, then optimize. We implemented SPSC queue in Phase 2 because it was straightforward and high-impact.

### Why is p50 = p99?
The histogram buckets are coarse at low latencies. Most operations complete in the same bucket (~32us). As we optimize, we'll see more spread. This is actually a good sign - consistent latency.

---

## Resources

- [How to Build a Fast Limit Order Book](https://web.archive.org/web/20110219163448/http://howtohft.wordpress.com/2011/02/15/how-to-build-a-fast-limit-order-book/)
- [Trading at Light Speed](https://www.youtube.com/watch?v=NH1Tta7purM) - David Gross, Meeting C++ 2022
- [Lock-Free Programming](https://preshing.com/20120612/an-introduction-to-lock-free-programming/)
- [Erik Rigtorp's SPSCQueue](https://github.com/rigtorp/SPSCQueue)
- [What Every Programmer Should Know About Memory](https://people.freebsd.org/~lstewart/articles/cpumemory.pdf)
