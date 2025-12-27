// tests/benchmarks/queue_bench.cpp
//
// Google Benchmark tests for SPSC queue performance
//

#include <benchmark/benchmark.h>
#include <thread>
#include <atomic>

#include "utils/spsc_queue.hpp"

using namespace exchange;

// ============================================================================
// Single-threaded benchmarks
// ============================================================================

static void BM_SPSCQueue_PushPop_SingleThread(benchmark::State& state) {
    SPSCQueue<int64_t, 65536> queue;
    int64_t value = 0;
    
    for (auto _ : state) {
        queue.try_push(value++);
        auto result = queue.try_pop();
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SPSCQueue_PushPop_SingleThread)->Unit(benchmark::kNanosecond);

static void BM_SPSCQueue_Push_UntilFull(benchmark::State& state) {
    for (auto _ : state) {
        SPSCQueue<int64_t, 1024> queue;
        
        for (int i = 0; i < 1023; ++i) {  // Capacity - 1
            queue.try_push(i);
        }
        
        benchmark::DoNotOptimize(queue.size_approx());
    }
    
    state.SetItemsProcessed(state.iterations() * 1023);
}
BENCHMARK(BM_SPSCQueue_Push_UntilFull)->Unit(benchmark::kMicrosecond);

static void BM_SPSCQueue_Pop_UntilEmpty(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        SPSCQueue<int64_t, 1024> queue;
        for (int i = 0; i < 1023; ++i) {
            queue.try_push(i);
        }
        state.ResumeTiming();
        
        while (auto val = queue.try_pop()) {
            benchmark::DoNotOptimize(val);
        }
    }
    
    state.SetItemsProcessed(state.iterations() * 1023);
}
BENCHMARK(BM_SPSCQueue_Pop_UntilEmpty)->Unit(benchmark::kMicrosecond);

// ============================================================================
// Multi-threaded benchmarks
// ============================================================================

static void BM_SPSCQueue_ProducerConsumer(benchmark::State& state) {
    const int64_t items_per_iter = 10000;
    
    for (auto _ : state) {
        SPSCQueue<int64_t, 65536> queue;
        std::atomic<bool> done{false};
        std::atomic<int64_t> consumed{0};
        
        // Consumer thread
        std::thread consumer([&]() {
            int64_t count = 0;
            while (!done.load(std::memory_order_relaxed) || !queue.empty()) {
                if (auto val = queue.try_pop()) {
                    benchmark::DoNotOptimize(*val);
                    ++count;
                }
            }
            consumed.store(count, std::memory_order_relaxed);
        });
        
        // Producer (this thread)
        for (int64_t i = 0; i < items_per_iter; ++i) {
            while (!queue.try_push(i)) {
                // Spin if full
            }
        }
        
        done.store(true, std::memory_order_relaxed);
        consumer.join();
    }
    
    state.SetItemsProcessed(state.iterations() * items_per_iter);
}
BENCHMARK(BM_SPSCQueue_ProducerConsumer)->Unit(benchmark::kMillisecond);

// ============================================================================
// Latency benchmarks
// ============================================================================

static void BM_SPSCQueue_Latency_Empty(benchmark::State& state) {
    SPSCQueue<int64_t, 65536> queue;
    
    for (auto _ : state) {
        auto result = queue.try_pop();
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SPSCQueue_Latency_Empty)->Unit(benchmark::kNanosecond);

static void BM_SPSCQueue_Latency_WithData(benchmark::State& state) {
    SPSCQueue<int64_t, 65536> queue;
    
    // Pre-fill with some data
    for (int i = 0; i < 1000; ++i) {
        queue.try_push(i);
    }
    
    int64_t counter = 1000;
    for (auto _ : state) {
        queue.try_push(counter++);
        auto result = queue.try_pop();
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SPSCQueue_Latency_WithData)->Unit(benchmark::kNanosecond);

// ============================================================================
// Different payload sizes
// ============================================================================

template <size_t Size>
struct Payload {
    char data[Size];
};

template <size_t PayloadSize>
static void BM_SPSCQueue_PayloadSize(benchmark::State& state) {
    SPSCQueue<Payload<PayloadSize>, 4096> queue;
    Payload<PayloadSize> payload{};
    
    for (auto _ : state) {
        queue.try_push(payload);
        auto result = queue.try_pop();
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() * PayloadSize);
}

BENCHMARK_TEMPLATE(BM_SPSCQueue_PayloadSize, 8)->Unit(benchmark::kNanosecond);
BENCHMARK_TEMPLATE(BM_SPSCQueue_PayloadSize, 64)->Unit(benchmark::kNanosecond);
BENCHMARK_TEMPLATE(BM_SPSCQueue_PayloadSize, 128)->Unit(benchmark::kNanosecond);
BENCHMARK_TEMPLATE(BM_SPSCQueue_PayloadSize, 256)->Unit(benchmark::kNanosecond);

BENCHMARK_MAIN();
