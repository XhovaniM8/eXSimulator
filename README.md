# Exchange Simulator

A high-performance limit order book matching engine in C++. Processes 3.3M orders/sec with 32us median latency on commodity ARM hardware.

## Current Status

Phase 2 complete, Phase 3 (benchmarking) in progress. See [ROADMAP.md](ROADMAP.md) for details.

**What works:**
- Full matching engine with price-time priority
- Order types: Limit, Market, IOC, FOK, PostOnly
- Cancel and replace orders
- Self-trade prevention
- Lock-free SPSC queue for order ingestion
- Multi-symbol support with sharding
- Trading agents: MarketMaker, Momentum, Noise
- Event journal for replay
- Latency histograms

**Performance (current):**
| Metric | Result |
|--------|--------|
| Throughput | 3.3M orders/sec |
| p50 Latency | 32us |
| p99 Latency | 32us |
| Trades/sec | 8,700 |

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         Gateway Layer                           │
│                  (FIX/Binary protocol - planned)                │
└─────────────────────────────────────────────────────────────────┘
                                 │
              ┌──────────────────┼──────────────────┐
              ▼                  ▼                  ▼
     ┌──────────────┐   ┌──────────────┐   ┌──────────────┐
     │  OrderBook   │   │  OrderBook   │   │  OrderBook   │
     │    AAPL      │   │    GOOGL     │   │    MSFT      │
     └──────────────┘   └──────────────┘   └──────────────┘
              │                  │                  │
              └──────────────────┼──────────────────┘
                                 ▼
┌─────────────────────────────────────────────────────────────────┐
│                        Event Journal                            │
│                    (append-only binary log)                     │
└─────────────────────────────────────────────────────────────────┘
```

## Build

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

## Run

```bash
# Main simulator
./exchange_sim --ticks 100000

# Feature tests
./feature_test

# Unit tests
./test_order_book
```

## Project Structure

```
src/
├── core/       # Order, Trade, Price, Quantity types
├── engine/     # MatchingEngine, OrderBook, PriceLevel
├── agents/     # MarketMaker, Momentum, Noise traders
├── replay/     # Event journal, replay harness
└── utils/      # SPSC queue, memory pool, timing, histograms

tests/
├── unit/           # Catch2 tests
└── benchmarks/     # Google Benchmark (in progress)
```

## References

- [How to Build a Fast Limit Order Book](https://web.archive.org/web/20110219163448/http://howtohft.wordpress.com/2011/02/15/how-to-build-a-fast-limit-order-book/)
- [Trading at Light Speed](https://www.youtube.com/watch?v=NH1Tta7purM) - David Gross, Meeting C++ 2022
- [Erik Rigtorp's SPSCQueue](https://github.com/rigtorp/SPSCQueue)
