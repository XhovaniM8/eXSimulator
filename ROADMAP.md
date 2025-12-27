# Roadmap

Phased development plan with realistic goals for learning.

**Last Updated**: December 2024  
**Status**: Phase 2 ~85% complete

---

## Phase 1: Foundation ✅ COMPLETE
**Duration**: 2 weeks → **Actual: ~1 week**

- [x] Get CMake build working
- [x] Implement `Order` struct with all fields (id, price, qty, side, type, tif)
- [x] Implement `OrderBook` with `std::unordered_map` for price levels
- [x] Basic `add_order()` and `match()` logic
- [x] Simple `main.cpp` that runs simulation
- [x] First unit test passing
- [x] `PriceLevel` struct with FIFO queue
- [x] `Symbol` type with fixed-size storage
- [x] Price/Quantity typedefs with scaling

**Deliverables**:
- `Order`, `Symbol`, `Price`, `Quantity` types in `core/`
- `OrderBook` with basic matching in `engine/`
- `main.cpp` runs and prints trades

---

## Phase 2: Correctness (Current)
**Duration**: 2 weeks → **Target: 1 more week**

### Completed ✅
- [x] `cancel_order()` functionality
- [x] Partial fills working correctly
- [x] Price-time priority (FIFO within price level)
- [x] Market orders
- [x] Limit orders
- [x] PostOnly orders (reject if would cross)
- [x] IOC (Immediate or Cancel)
- [x] FOK (Fill or Kill)
- [x] Replace order (same price - amend in place)
- [x] Replace order (different price - cancel/replace)
- [x] Self-trade prevention (basic implementation)
- [x] Unit tests: 16 test cases in `test_order_book.cpp`
- [x] Feature test executable

### Remaining 🔧
- [ ] **Fix CI build** (std::hardware_destructive_interference_size warning)
- [ ] Edge case: empty book matching
- [ ] Edge case: exact fill at multiple levels
- [ ] Edge case: cancel non-existent order
- [ ] Verify `PriceLevel::modify_quantity()` exists and works
- [ ] Add TraderId field for proper self-trade prevention
- [ ] CI passing on every push

### Known Issues 🐛
| Issue | Priority | Status |
|-------|----------|--------|
| `CACHE_LINE_SIZE` uses non-portable std constant | P0 | Blocking CI |
| `InboundMessage` wastes memory (no union) | P1 | Open |
| Self-trade uses order ID proximity hack | P2 | Documented |
| `get_bids/get_asks` allocates on every call | P3 | Defer to Phase 4 |

**Success metric**: CI green, all unit tests pass

**Target performance**: 10K orders/sec (don't optimize yet)

---

## Phase 3: First Benchmark
**Duration**: 1 week

- [ ] Add basic timing with `std::chrono` (already have `Timing` class)
- [ ] Simple benchmark: insert N orders, measure wall time
- [ ] Generate flamegraph to identify bottlenecks
- [ ] Document baseline numbers
- [ ] Add Google Benchmark integration

**Deliverables**:
- `benchmarks/` directory with repeatable tests
- Baseline numbers documented in `docs/PERFORMANCE.md`
- Flamegraph SVG checked into repo

**Expected baseline**:
| Metric | Target |
|--------|--------|
| Throughput | 10K-50K orders/sec |
| Latency | 10-100µs |

---

## Phase 4: Data Structure Optimization
**Duration**: 1-2 weeks

- [ ] Profile and identify hot paths
- [ ] Consider `std::map` for sorted price iteration (get_bids/get_asks)
- [ ] Add cached best bid/ask update optimization
- [ ] Optimize `PriceLevel` queue implementation
- [ ] Re-run benchmarks, compare to baseline

**Target performance**: 100K orders/sec, <50µs p50 latency

---

## Phase 5: Memory & Cache
**Duration**: 1-2 weeks

- [ ] Implement memory pool for Order allocation
- [ ] Cache-line align hot structures (`alignas(64)`)
- [ ] Separate hot/cold fields in Order struct
- [ ] Profile cache misses with `perf stat`
- [ ] Restore `InboundMessage` union for compact messages

**Target performance**: 200K-500K orders/sec, <20µs p50 latency

---

## Phase 6: Lock-Free Queue
**Duration**: 1-2 weeks

- [ ] Fix SPSC queue implementation (currently has CI issues)
- [ ] Understand memory ordering in lock-free code
- [ ] Separate threads: order input → matching → output
- [ ] Measure end-to-end latency across threads

**Target performance**: 500K+ orders/sec, <10µs p50 latency

---

## Phase 7: Agents & Simulation
**Duration**: 1-2 weeks

- [ ] Implement `NoiseTrader` (random orders)
- [ ] Implement basic `MarketMaker` (spread quoting, inventory skew)
- [ ] Run simulation with multiple agents
- [ ] Log trades, verify market behavior makes sense

**Success metric**: Multi-hour simulation without crashes

---

## Phase 8: Event Journal & Replay
**Duration**: 1 week

- [ ] Binary event log working (already have `EventJournal` class)
- [ ] Replay harness reads log and re-executes
- [ ] Verify replay produces identical output
- [ ] Add checksum verification (partially implemented)

**Success metric**: Deterministic replay, byte-for-byte reproducibility

---

## Phase 9: Dashboard
**Duration**: 2-3 weeks

- [ ] WebSocket server for market data
- [ ] REST API for start/stop/config
- [ ] React dashboard with order book display
- [ ] Real-time latency charts

**Success metric**: Browser shows live order book

---

## Phase 10: Polish
**Duration**: 1-2 weeks

- [ ] Code cleanup and documentation
- [ ] Design doc explaining tradeoffs
- [ ] Performance write-up with charts
- [ ] README with impressive numbers

---

## Revised Timeline

| Phase | Original | Revised | Status |
|-------|----------|---------|--------|
| 1. Foundation | Weeks 1-2 | Week 1 | ✅ Done |
| 2. Correctness | Weeks 3-4 | Week 2 | 🔧 85% |
| 3. Benchmark | Weeks 5-6 | Week 3 | ⏳ Next |
| 4. Data Structures | Weeks 7-8 | Weeks 4-5 | |
| 5. Memory & Cache | Weeks 9-10 | Weeks 5-6 | |
| 6. Lock-Free | Weeks 11-12 | Weeks 6-7 | |
| 7. Agents | Weeks 13-14 | Weeks 7-8 | |
| 8. Replay | Weeks 15-16 | Week 9 | |
| 9. Dashboard | Weeks 17-20 | Weeks 10-12 | |
| 10. Polish | Weeks 21+ | Week 13 | |

**New Total**: ~13 weeks (3 months) vs original 20+ weeks

---

## Final Targets

| Metric | Realistic Target | Stretch Goal |
|--------|-----------------|--------------|
| Throughput | 500K orders/sec | 1M+ orders/sec |
| p50 Latency | <10µs | <1µs |
| p99 Latency | <100µs | <10µs |
| Memory/Order | <128 bytes | <64 bytes |

---

## FAQ

### Why not start with lock-free queues?
Lock-free code is hard to debug. Get correctness first, then optimize. You'll learn more by seeing the progression.

### What if I can't hit the targets?
The learning matters more than the numbers. Being able to explain *why* you're at 100K orders/sec and *what* you'd do to reach 1M is more impressive than cargo-culting optimizations you don't understand.

### How do I explain this in interviews?
Focus on:
1. Data structure choices and tradeoffs
2. Bottlenecks you found and how you fixed them
3. What you'd do differently next time
4. Specific numbers and how you measured them
