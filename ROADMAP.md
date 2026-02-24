# Exchange Simulator Roadmap

## Current Status: Phase 4 Next

**Last Updated**: February 2026

### Performance Achieved (Release Build, Single Core)
| Metric | Result | Original Target | Status |
|--------|--------|-----------------|--------|
| AddOrder throughput | **5.8M/s** | 10K/sec | 580x target |
| Matching throughput (100K) | **9.7M/s** | 10K/sec | 970x target |
| Batch processing | **6.0M/s** | - | - |
| Cancel order | **180K/s** | - | Bottleneck |
| SPSC queue | **610M/s** | - | Excellent |

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

### Known Bottleneck
Cancel order is 30x slower than add (180K/s vs 5.8M/s) due to O(n) linear scan
in `PriceLevel::find_node()`. Fix: replace `std::vector<OrderNode*>` with
`std::unordered_map<OrderId, OrderNode*>` for O(1) lookup. Targeted in Phase 4.

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

**Known Issues (deferred to Phase 4)**:
- Self-trade prevention uses order ID proximity as proxy for trader ID
- `get_bids()`/`get_asks()` allocate on each call
- Cancel is O(n) due to linear scan in PriceLevel

### Phase 3: Benchmarking [Complete]
**Duration**: 1 week

- [x] Baseline measurement: **5.8M orders/sec** (AddOrder), **9.7M/s** (matching at scale)
- [x] Google Benchmark integration (order book, matching engine, queue benchmarks)
- [x] Release build from source (removed debug-build system library)
- [x] CPU pinning with taskset for stable measurements
- [x] Identified hotspot: cancel O(n) scan is 30x slower than add
- [ ] Google Benchmark in CI
- [ ] Profiling with perf/flamegraph
- [ ] Document baseline in performance report

**Run benchmarks**:
```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON ..
make -j$(nproc)
taskset -c 0 ./benchmarks
```

### Phase 4: Data Structure Optimization [Next]
**Duration**: 2 weeks
**Target**: 2M+ cancel/sec, 10M+ add/sec

- [ ] Replace `std::vector<OrderNode*>` in PriceLevel with `std::unordered_map<OrderId, OrderNode*>` → O(1) cancel
- [ ] Fix self-trade prevention to use real trader ID field
- [ ] Replace `std::unordered_map` for price levels with flat hashmap
- [ ] Fix `get_bids()`/`get_asks()` to avoid per-call allocation
- [ ] Pool allocator for OrderNode objects
- [ ] Profile cache misses with `perf stat`

### Phase 5: Memory & Cache Optimization [Pending]
**Duration**: 1-2 weeks
**Target**: 15M+ orders/sec, <5us p50

- [ ] Hot/cold data separation in Order struct
- [ ] Prefetch hints for likely code paths
- [ ] Measure cache hit rates with `perf stat -d`
- [ ] Consider array-based price level for fixed price ranges

### Phase 6: Lock-Free & Concurrency [Pending]
**Duration**: 1-2 weeks
**Target**: Maintain throughput under contention

- [x] SPSC queue already implemented (610M/s)
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
| 4. Data Structures | 2 weeks | - | Next |
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
| **3 (Actual baseline)** | **5.8M/sec** | **180K/sec** | **~172ns** |
| 4 (Cancel fix) | 6M+/sec | 2M+/sec | <150ns |
| 5 (Cache opt) | 10M+/sec | 3M+/sec | <100ns |
| 6 (Final) | 15M+/sec | 5M+/sec | <50ns |

---

## FAQ

### Why not start with lock-free queues?
Lock-free code is hard to debug. Get correctness first, then optimize. SPSC queue was implemented in Phase 2 because it was straightforward and high-impact.

### Why is cancel so much slower than add?
Cancel requires finding an order by ID within a price level. Currently implemented as a linear scan (`O(n)`). Fix is to add a hashmap from OrderId to OrderNode pointer inside PriceLevel, making it O(1). This is the Phase 4 priority.

### Are these benchmark numbers realistic?
The micro-benchmarks (5.8M/s add) are best-case with hot cache. Real throughput under load with network I/O, validation, and market data publishing is closer to 1-3M orders/sec — still competitive with production systems at smaller exchanges.

---

## Resources

- [How to Build a Fast Limit Order Book](https://web.archive.org/web/20110219163448/http://howtohft.wordpress.com/2011/02/15/how-to-build-a-fast-limit-order-book/)
- [Trading at Light Speed](https://www.youtube.com/watch?v=NH1Tta7purM) - David Gross, Meeting C++ 2022
- [Lock-Free Programming](https://preshing.com/20120612/an-introduction-to-lock-free-programming/)
- [Erik Rigtorp's SPSCQueue](https://github.com/rigtorp/SPSCQueue)
- [What Every Programmer Should Know About Memory](https://people.freebsd.org/~lstewart/articles/cpumemory.pdf)
- [CppCon: When a Microsecond is an Eternity](https://www.youtube.com/watch?v=NH1Tta7purM) - Carl Cookmory](https://people.freebsd.org/~lstewart/articles/cpumemory.pdf)
