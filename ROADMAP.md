# Exchange Simulator Roadmap

## Current Status: Phase 4 In Progress

**Last Updated**: March 2026

### Performance Achieved (Release Build, Single Core, i7-9700F)
| Metric | Result | Original Target | Status |
|--------|--------|-----------------|--------|
| AddOrder throughput | **3.56M/s** | 10K/sec | 356x target |
| AddOrder (always match) | **30.5M/s** | - | Hot path |
| Matching throughput (100K) | **7.70M/s** | 10K/sec | 770x target |
| Batch processing (10K) | **4.61M/s** | - | - |
| Cancel order | **5.93M/s** | - | 33x improvement |
| SPSC queue | **596M/s** | - | Excellent |

### What's Working
- Full matching engine with price-time priority
- All order types: Limit, Market, IOC, FOK, PostOnly
- Self-trade prevention (basic)
- SPSC lock-free queue for order ingestion
- Cache-line aligned Order struct (64 bytes)
- Multi-symbol support with sharding
- Trading agents: MarketMaker, Momentum, Noise
- Latency histograms
- Google Benchmark micro-benchmarks
- CI/CD pipeline
- 37/37 unit tests passing
- O(1) cancel via `unordered_map` node index in PriceLevel
- O(1) best bid/ask via `std::map` iterator endpoints

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
- [x] Unit tests for all order types (37/37 passing)
- [x] Feature test executable

### Phase 3: Benchmarking [Complete]
**Duration**: 1 week

- [x] Baseline measurement: 3.56M orders/sec (AddOrder), 7.70M/s (matching at scale)
- [x] Google Benchmark integration (order book, matching engine, queue benchmarks)
- [x] Release build with performance governor (`cpupower frequency-set -g performance`)
- [x] CPU pinning with taskset for stable measurements
- [x] Identified hotspot: cancel O(n) scan, 72% of all CPU time
- [x] Profiling with perf + flamegraph
- [x] Cache miss analysis with `perf stat`
- [ ] Google Benchmark in CI
- [ ] Document baseline in performance report

### Phase 4: Data Structure Optimization [In Progress]
**Duration**: 2 weeks
**Target**: 2M+ cancel/sec, 10M+ add/sec

- [x] Replace `std::vector<OrderNode*>` in PriceLevel with `unordered_map<OrderId, OrderNode*>` → O(1) cancel
- [x] Swap `bid_levels_`/`ask_levels_` to `std::map` → O(1) BBO update via iterator endpoints
- [x] Profile with perf + flamegraph — cancel_order CPU%: 72% → 10%
- [x] Throughput: 2,903 → 7,726 ticks/sec (2.66x)
- [x] Cancel: 180K/s → 5.93M/s (33x)
- [ ] Pool allocator for OrderNode (eliminate heap alloc on hot path)
- [ ] Identity hash for OrderId in node_index_ (avoid default hash overhead)
- [ ] Fix self-trade prevention to use real trader ID field
- [ ] Fix `get_bids()`/`get_asks()` to avoid per-call allocation

### Phase 5: Memory & Cache Optimization [Pending]
**Duration**: 1-2 weeks
**Target**: 15M+ orders/sec, <5us p50

Current cache stats (i7-9700F):
- IPC: 1.15 (target: 2.0+)
- L1 miss rate: 5.65%
- LLC miss rate: 1.36%

- [ ] Pool allocator for OrderNode — eliminate random heap addresses causing L1 misses
- [ ] Hot/cold data separation in Order struct
- [ ] Prefetch hints for likely code paths
- [ ] Consider flat array index for small price levels (< 8 orders)
- [ ] Fix latency histogram quantization (p50/p90/p99 currently showing identical values)

### Phase 6: Lock-Free & Concurrency [Pending]
**Duration**: 1-2 weeks
**Target**: Maintain throughput under contention

- [x] SPSC queue already implemented (596M/s)
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
| 3. Benchmarking | 1 week | 1 week | Complete |
| 4. Data Structures | 2 weeks | In progress | In Progress |
| 5. Memory & Cache | 2 weeks | - | Pending |
| 6. Lock-Free | 2 weeks | - | Pending |
| 7. Agents | 1 week | - | Pending |
| 8. Replay | 1 week | - | Pending |
| 9. Dashboard | 3 weeks | - | Pending |
| 10. Polish | 1 week | - | Pending |

---

## Performance Targets by Phase

| Phase | AddOrder | Cancel | p50 Latency |
|-------|----------|--------|-------------|
| 2 (Original target) | 10K/sec | - | <100us |
| **3 (Baseline)** | **3.56M/sec** | **180K/sec** | ~172ns |
| **4 (In progress)** | **3.56M/sec** | **5.93M/sec** | TBD |
| 5 (Cache opt) | 10M+/sec | 8M+/sec | <100ns |
| 6 (Final) | 15M+/sec | 10M+/sec | <50ns |

---

## Resources

- [How to Build a Fast Limit Order Book](https://web.archive.org/web/20110219163448/http://howtohft.wordpress.com/2011/02/15/how-to-build-a-fast-limit-order-book/)
- [Trading at Light Speed](https://www.youtube.com/watch?v=NH1Tta7purM) - David Gross, Meeting C++ 2022
- [Lock-Free Programming](https://preshing.com/20120612/an-introduction-to-lock-free-programming/)
- [Erik Rigtorp's SPSCQueue](https://github.com/rigtorp/SPSCQueue)
- [What Every Programmer Should Know About Memory](https://people.freebsd.org/~lstewart/articles/cpumemory.pdf)
