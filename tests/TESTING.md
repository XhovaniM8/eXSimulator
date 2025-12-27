# Testing Guide

## Overview

This document describes the testing strategy for the exchange simulator.

## Test Categories

### 1. Unit Tests (`tests/`)

Test individual components in isolation.

| File | Component | Coverage |
|------|-----------|----------|
| `test_order_book.cpp` | OrderBook | Basic operations |
| `test_order_book_comprehensive.cpp` | OrderBook | Full coverage including edge cases |

### 2. Feature Tests (`src/feature_test.cpp`)

Quick smoke tests for major features:
- Limit order matching
- Market orders
- PostOnly rejection
- IOC/FOK behavior
- Self-trade prevention

### 3. Integration Tests

End-to-end tests with multiple components.

### 4. Benchmarks (`tests/benchmarks/`)

Performance tests with Google Benchmark.

---

## Running Tests

### Build and run all tests

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# Unit tests
./bin/test_order_book

# Comprehensive tests  
./bin/test_order_book_comprehensive

# Feature tests
./bin/feature_test
```

### Run with sanitizers

```bash
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined" ..
make
ASAN_OPTIONS=detect_leaks=1 ./bin/test_order_book
```

### Run with coverage

```bash
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="--coverage" ..
make
./bin/test_order_book
gcov src/engine/order_book.cpp
```

---

## Test Philosophy

### Independence

Each test starts with a fresh `OrderBook` instance. Tests do not depend on each other.

### Behavior, Not Implementation

Tests verify observable behavior, not internal data structure choices. This allows refactoring without breaking tests.

```cpp
// Good: Tests behavior
ASSERT_EQ(book->best_bid(), 10000);

// Bad: Tests implementation
ASSERT_EQ(book->bid_levels_.size(), 1);
```

### Edge Cases

Every feature has tests for:
- Normal operation
- Boundary conditions
- Error conditions
- Empty/null states

### Determinism

Tests use fixed random seeds when randomness is needed:

```cpp
std::mt19937 rng(42);  // Always reproducible
```

---

## Test Coverage Requirements

### Phase 2 Minimum Coverage

| Component | Required | Current |
|-----------|----------|---------|
| Order types (Limit, Market, IOC, FOK, PostOnly) | 100% | ✓ |
| Matching logic | 100% | ✓ |
| Cancel | 100% | ✓ |
| Replace | 100% | ✓ |
| Price-time priority | 100% | ✓ |
| Edge cases | 80% | ~70% |

### Adding New Tests

1. Create test in appropriate file
2. Follow naming convention: `TEST(feature_behavior_expected)`
3. Include both success and failure paths
4. Document any non-obvious test logic

---

## CI Pipeline

### Triggered On
- Push to `main` or `develop`
- Pull requests to `main`

### Jobs

1. **build-and-test**: Compile and run tests (GCC + Clang)
2. **sanitizers**: Run with AddressSanitizer and UBSan
3. **static-analysis**: Run cppcheck
4. **docs**: Verify documentation exists

### CI Configuration

See `.github/workflows/ci.yml`

---

## Common Issues

### CI Fails with "interference-size" Warning

The `std::hardware_destructive_interference_size` constant triggers warnings on some compilers. Fix: use a hardcoded `CACHE_LINE_SIZE = 64`.

### Tests Fail with ASAN

Usually indicates:
- Use after free
- Buffer overflow
- Memory leak

Run locally with ASAN to get detailed stack traces.

### Flaky Tests

If tests pass locally but fail in CI:
- Check for race conditions (shouldn't exist in unit tests)
- Check for uninitialized memory
- Check for order-dependent tests

---

## Future Testing Plans

### Phase 3: Benchmarks
- Add Google Benchmark integration
- Throughput tests (orders/sec)
- Latency histograms

### Phase 6: Concurrency Tests
- Multi-threaded stress tests
- Lock-free queue correctness
- Thread sanitizer (TSan)

### Phase 8: Replay Tests
- Deterministic replay verification
- Journal corruption handling
