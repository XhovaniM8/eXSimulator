# Roadmap

Phased development plan with realistic goals for learning.

## Phase 1: Foundation (Weeks 1-2)
**Goal: Basic matching that compiles and runs**

- [ ] Get CMake build working on your machine
- [ ] Implement `Order` struct (just the basics: id, price, qty, side)
- [ ] Implement `OrderBook` with `std::map` for price levels (not optimal, but simple)
- [ ] Basic `add_order()` and `match()` logic
- [ ] Simple `main.cpp` that adds a few orders and prints trades
- [ ] First unit test (any test that passes)

**Success metric**: Run `./exchange_sim` and see "Trade executed" printed.

**Target performance**: Don't measure yet. Just make it work.

## Phase 2: Correctness (Weeks 3-4)
**Goal: Matching logic is correct and tested**

- [ ] Add `cancel_order()` functionality
- [ ] Handle partial fills correctly
- [ ] Price-time priority (FIFO within price level)
- [ ] Unit tests for all order types: market, limit, cancel
- [ ] Edge cases: empty book, self-match, exact fills
- [ ] CI passing on every push

**Success metric**: 100% of unit tests pass, including edge cases.

**Target performance**: 10K orders/sec (very achievable with naive implementation)

## Phase 3: First Benchmark (Weeks 5-6)
**Goal: Measure baseline, identify bottlenecks**

- [ ] Add basic timing (just `std::chrono`, not rdtsc yet)
- [ ] Simple benchmark: insert N orders, measure wall time
- [ ] Generate flamegraph to see where time goes
- [ ] Identify the slow parts (probably `std::map` lookups)

**Success metric**: Know your baseline numbers. Write them down.

**Expected baseline** (before optimization):
| Metric | Realistic First Attempt |
|--------|------------------------|
| Throughput | 10K-50K orders/sec |
| Latency | 10-100µs |

## Phase 4: Data Structure Optimization (Weeks 7-8)
**Goal: Replace slow structures with faster ones**

- [ ] Replace `std::map` with `std::unordered_map` for price levels
- [ ] Add cached best bid/ask pointers
- [ ] Implement `PriceLevel` as a proper queue (not just vector)
- [ ] Re-run benchmarks, compare to baseline

**Target performance**: 100K orders/sec, <50µs p50 latency

## Phase 5: Memory & Cache (Weeks 9-10)
**Goal: Eliminate allocations on hot path**

- [ ] Implement simple memory pool for orders
- [ ] Cache-line align hot structures (`alignas(64)`)
- [ ] Separate hot/cold fields in Order struct
- [ ] Profile cache misses with `perf stat`

**Target performance**: 200K-500K orders/sec, <20µs p50 latency

## Phase 6: Lock-Free Queue (Weeks 11-12)
**Goal: Multi-threaded architecture**

- [ ] Implement SPSC queue (copy from Rigtorp, understand it)
- [ ] Separate threads: order input → matching → output
- [ ] Measure end-to-end latency across threads

**Target performance**: 500K+ orders/sec, <10µs p50 latency

## Phase 7: Agents & Simulation (Weeks 13-14)
**Goal: Realistic order flow**

- [ ] Implement `NoiseTrader` (random orders)
- [ ] Implement basic `MarketMaker` (simple spread)
- [ ] Run simulation with multiple agents
- [ ] Log trades, verify market makes sense

**Success metric**: Multi-hour simulation runs without crashes

## Phase 8: Event Journal & Replay (Weeks 15-16)
**Goal: Deterministic replay for debugging**

- [ ] Binary event log (append-only file)
- [ ] Replay harness that reads log and re-executes
- [ ] Verify replay produces identical output

**Success metric**: Run simulation, replay from log, get identical trades

## Phase 9: Dashboard (Weeks 17-20)
**Goal: Visualization and control**

- [ ] WebSocket server for market data
- [ ] REST API for start/stop/config
- [ ] React dashboard with order book display
- [ ] Real-time latency charts

**Success metric**: Open browser, see live order book updating

## Phase 10: Polish (Weeks 21+)
**Goal: Interview-ready**

- [ ] Clean up code, add comments
- [ ] Write design doc explaining tradeoffs
- [ ] Performance write-up with charts
- [ ] README with impressive numbers

---

## Realistic Final Targets

After 4-6 months of work, aim for:

| Metric | Realistic Target | Stretch Goal |
|--------|-----------------|--------------|
| Throughput | 500K orders/sec | 1M+ orders/sec |
| p50 Latency | <10µs | <1µs |
| p99 Latency | <100µs | <10µs |
| Memory/Order | <128 bytes | <64 bytes |

**The stretch goals are what top HFT firms achieve.** Getting to the realistic targets is already impressive and interview-worthy.

---

## FAQ

### Why not Redis?
Redis adds ~100µs network latency per operation. The whole point of this project is sub-10µs latency. Redis is for distributed systems, not single-process matching engines. Your event journal handles persistence.

### Why not start with lock-free queues?
Lock-free code is hard to debug. Get correctness first with simple `std::mutex` if needed, then optimize. You'll learn more by seeing the progression.

### What if I can't hit the targets?
The learning matters more than the numbers. Being able to explain *why* you're at 100K orders/sec and *what* you'd do to reach 1M is more impressive than cargo-culting optimizations you don't understand.

### How do I explain this in interviews?
Focus on:
1. Data structure choices and tradeoffs
2. Bottlenecks you found and how you fixed them
3. What you'd do differently next time
4. Specific numbers and how you measured them
