# Exchange Simulator Design Document

## Overview

This document describes the design of a high-performance exchange simulator with matching engine, suitable for demonstrating quant-dev skills to prop trading firms.

## Goals

1. **Correctness under concurrency**: Lock-free data structures, deterministic replay
2. **High-performance C++**: Cache-friendly layouts, SPSC queues, zero-copy
3. **Systems design**: Event sourcing, symbol sharding, backpressure
4. **Measurable results**: Orders/sec, p50/p99 latency, memory footprint
5. **Full-stack integration**: WebSocket, REST, real-time dashboard

## Architecture

### Order Flow

```
Client Order → Gateway → Inbound Queue → Matching Engine → Outbound Events
                                              ↓
                                        Order Book
                                              ↓
                                         Journal
```

### Threading Model

```
Thread 1: Gateway (network I/O)
    ↓ SPSC Queue
Thread 2: Matching Engine (order processing)
    ↓ SPSC Queue
Thread 3: Market Data Publisher
    ↓ SPSC Queue  
Thread 4: Journal Writer (disk I/O)
```

### Data Structures

#### Order Book
- **Bid/Ask sides**: Separate hash maps from price → PriceLevel
- **PriceLevel**: Intrusive doubly-linked list for O(1) cancel
- **Best price**: Cached for O(1) access
- **Order storage**: Flat map from OrderId → Order

#### Price Level Queue (FIFO)
```cpp
struct PriceLevel {
    Price price;
    Quantity total_quantity;
    OrderNode* head;  // Front of queue (first to fill)
    OrderNode* tail;  // Back of queue (new orders)
    vector<OrderNode*> nodes;  // For O(1) cancel lookup
};
```

#### Order Layout (64 bytes, cache-line aligned)
```cpp
struct alignas(64) Order {
    // Hot fields (32 bytes)
    OrderId id;           // 8
    Price price;          // 8
    Quantity quantity;    // 4
    Quantity filled_qty;  // 4
    Side side;            // 1
    OrderType type;       // 1
    TimeInForce tif;      // 1
    OrderStatus status;   // 1
    uint32_t _pad;        // 4
    
    // Cold fields
    Symbol symbol;        // 8
    Timestamp timestamp;  // 8
    // Total: 48 bytes + padding = 64 bytes
};
```

### Memory Management

#### Memory Pool
- Pre-allocated fixed-size pools for orders
- Free-list for O(1) alloc/dealloc
- No heap allocation on hot path

#### SPSC Queue
- Lock-free bounded ring buffer
- Cache-line padded indices
- Local index caching for reduced contention

## Performance Targets

| Metric | Target | Notes |
|--------|--------|-------|
| Throughput | 1M+ orders/sec | Single symbol, single thread |
| p50 Latency | <1µs | Order submission to ack |
| p99 Latency | <10µs | Including worst-case matching |
| Memory/Order | <64 bytes | Cache-line aligned |
| Replay Determinism | 100% | Bit-for-bit reproducible |

## Key Optimizations

### Cache Efficiency
- Cache-line aligned structures
- Hot/cold field separation
- Contiguous memory for price levels

### Lock-Free Design
- SPSC queues between threads
- Atomic sequence numbers
- No mutexes on hot path

### Compile-Time
- Template metaprogramming for type safety
- `constexpr` where possible
- `-fno-exceptions -fno-rtti` for minimal overhead

### Memory
- Pre-allocated pools
- Custom allocators
- Huge pages (optional)

## Deterministic Replay

### Event Journal Format
```
+-------------------+
| JournalHeader     | 24 bytes
+-------------------+
| Payload           | variable
+-------------------+
```

### Replay Guarantees
1. Same sequence number → same output
2. Same order IDs for generated events
3. Same matching outcomes
4. Verifiable via hash comparison

## Benchmarking

### Latency Measurement
- rdtsc for cycle-accurate timing
- Calibrated to nanoseconds
- HDR histogram for percentiles

### Metrics Collected
- Order latency (submit → ack)
- Match latency (order → fill)
- Tick latency (all agents processed)
- Throughput (orders/sec)
- Memory usage

### Profiling
- perf + FlameGraph for CPU hotspots
- cachegrind for cache analysis
- perf stat for hardware counters

## Future Enhancements

### Phase 2
- Multi-symbol sharding
- NUMA-aware allocation
- Kernel bypass (DPDK/io_uring)

### Phase 3
- FIX protocol support
- Persistent order book
- Distributed deployment

## References

- [How to Build a Fast Limit Order Book](https://web.archive.org/web/20110219163448/http://howtohft.wordpress.com/2011/02/15/how-to-build-a-fast-limit-order-book/)
- [Trading at Light Speed - David Gross](https://www.youtube.com/watch?v=NH1Tta7purM)
- [Lock-Free Programming - Preshing](https://preshing.com/20120612/an-introduction-to-lock-free-programming/)
- [Erik Rigtorp's HFT Components](https://rigtorp.se/)
