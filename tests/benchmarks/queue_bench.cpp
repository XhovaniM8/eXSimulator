#include <benchmark/benchmark.h>
#include "utils/spsc_queue.hpp"

using namespace exchange;

// Benchmark: Single push
static void BM_SPSCQueue_Push(benchmark::State& state) {
    SPSCQueue<int, 65536> queue;
    
    int value = 42;
    for (auto _ : state) {
        bool success = queue.try_push(value);
        if (!success) {
            // Queue full, drain it
            int dummy;
            while (queue.try_pop(dummy)) {}
        }
        benchmark::DoNotOptimize(success);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SPSCQueue_Push);

// Benchmark: Single pop
static void BM_SPSCQueue_Pop(benchmark::State& state) {
    SPSCQueue<int, 65536> queue;
    
    // Pre-fill queue
    for (int i = 0; i < 65535; ++i) {
        queue.try_push(i);
    }
    
    for (auto _ : state) {
        int value = 0;
        bool success = queue.try_pop(value);
        if (!success) {
            // Queue empty, refill it
            for (int i = 0; i < 65535; ++i) {
                queue.try_push(i);
            }
        }
        benchmark::DoNotOptimize(value);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SPSCQueue_Pop);

// Benchmark: Push then pop (round trip)
static void BM_SPSCQueue_RoundTrip(benchmark::State& state) {
    SPSCQueue<int, 65536> queue;
    
    int value = 42;
    for (auto _ : state) {
        queue.try_push(value);
        int result = 0;
        queue.try_pop(result);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations() * 2);
}
BENCHMARK(BM_SPSCQueue_RoundTrip);

// Benchmark: Batch push
static void BM_SPSCQueue_BatchPush(benchmark::State& state) {
    SPSCQueue<int, 65536> queue;
    const int batch_size = 100;
    
    for (auto _ : state) {
        state.PauseTiming();
        // Clear queue if needed
        int dummy;
        while (queue.try_pop(dummy)) {}
        state.ResumeTiming();
        
        for (int i = 0; i < batch_size; ++i) {
            queue.try_push(i);
        }
    }
    
    state.SetItemsProcessed(state.iterations() * batch_size);
}
BENCHMARK(BM_SPSCQueue_BatchPush);

// Benchmark: Batch pop
static void BM_SPSCQueue_BatchPop(benchmark::State& state) {
    SPSCQueue<int, 65536> queue;
    const int batch_size = 100;
    
    for (auto _ : state) {
        state.PauseTiming();
        // Fill queue
        for (int i = 0; i < batch_size; ++i) {
            queue.try_push(i);
        }
        state.ResumeTiming();
        
        for (int i = 0; i < batch_size; ++i) {
            int value = 0;
            queue.try_pop(value);
            benchmark::DoNotOptimize(value);
        }
    }
    
    state.SetItemsProcessed(state.iterations() * batch_size);
}
BENCHMARK(BM_SPSCQueue_BatchPop);

// Benchmark: Front peek
static void BM_SPSCQueue_Front(benchmark::State& state) {
    SPSCQueue<int, 65536> queue;
    queue.try_push(42);
    
    for (auto _ : state) {
        const int* front = queue.front();
        benchmark::DoNotOptimize(front);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SPSCQueue_Front);

// Benchmark: Size query
static void BM_SPSCQueue_Size(benchmark::State& state) {
    SPSCQueue<int, 65536> queue;
    
    // Half-fill queue
    for (int i = 0; i < 32768; ++i) {
        queue.try_push(i);
    }
    
    for (auto _ : state) {
        size_t size = queue.size();
        benchmark::DoNotOptimize(size);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SPSCQueue_Size);

// Benchmark: Empty check
static void BM_SPSCQueue_Empty(benchmark::State& state) {
    SPSCQueue<int, 65536> queue;
    queue.try_push(42);
    
    for (auto _ : state) {
        bool empty = queue.empty();
        benchmark::DoNotOptimize(empty);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SPSCQueue_Empty);

// Benchmark: Larger element type (simulating Order struct)
struct LargeElement {
    uint64_t id;
    uint64_t price;
    uint32_t quantity;
    uint32_t filled_qty;
    uint8_t side;
    uint8_t type;
    uint8_t tif;
    uint8_t status;
    uint64_t timestamp;
    uint64_t symbol;
    char padding[16];  // To make it 64 bytes
};

static void BM_SPSCQueue_LargeElement(benchmark::State& state) {
    SPSCQueue<LargeElement, 65536> queue;
    
    LargeElement elem{};
    elem.id = 42;
    elem.price = 10000;
    elem.quantity = 100;
    
    for (auto _ : state) {
        queue.try_push(elem);
        LargeElement result;
        queue.try_pop(result);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations() * 2);
}
BENCHMARK(BM_SPSCQueue_LargeElement);

// Benchmark: High contention (alternating push/pop)
static void BM_SPSCQueue_Contention(benchmark::State& state) {
    SPSCQueue<int, 65536> queue;
    
    // Pre-fill half way
    for (int i = 0; i < 32768; ++i) {
        queue.try_push(i);
    }
    
    int push_val = 0;
    for (auto _ : state) {
        queue.try_push(push_val++);
        int pop_val = 0;
        queue.try_pop(pop_val);
        benchmark::DoNotOptimize(pop_val);
    }
    
    state.SetItemsProcessed(state.iterations() * 2);
}
BENCHMARK(BM_SPSCQueue_Contention);

