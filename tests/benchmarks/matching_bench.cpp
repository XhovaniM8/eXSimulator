#include <benchmark/benchmark.h>
#include "engine/matching_engine.hpp"
#include "core/order.hpp"

using namespace exchange;

// Benchmark: Submit orders to matching engine
static void BM_MatchingEngine_SubmitOrder(benchmark::State& state) {
    EngineConfig config;
    MatchingEngine engine(config);
    
    Symbol symbol("AAPL");
    engine.add_symbol(symbol);
    
    OrderId order_id = 1;
    for (auto _ : state) {
        Order order(order_id++, symbol, Side::Buy, 10000 + (order_id % 100), 100);
        auto result = engine.submit_order(order);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MatchingEngine_SubmitOrder);

// Benchmark: Submit and match orders
static void BM_MatchingEngine_Matching(benchmark::State& state) {
    EngineConfig config;
    MatchingEngine engine(config);
    
    Symbol symbol("AAPL");
    engine.add_symbol(symbol);
    
    // Pre-populate with sell orders
    for (int i = 0; i < 100; ++i) {
        Order sell(i, symbol, Side::Sell, 10100 + i, 100);
        engine.submit_order(sell);
    }
    
    OrderId order_id = 1000;
    for (auto _ : state) {
        Order buy(order_id++, symbol, Side::Buy, 10100, 50);
        auto result = engine.submit_order(buy);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MatchingEngine_Matching);

// Benchmark: Cancel orders
static void BM_MatchingEngine_CancelOrder(benchmark::State& state) {
    EngineConfig config;
    MatchingEngine engine(config);
    
    Symbol symbol("AAPL");
    engine.add_symbol(symbol);
    
    for (auto _ : state) {
        state.PauseTiming();
        Order order(1, symbol, Side::Buy, 10000, 100);
        engine.submit_order(order);
        state.ResumeTiming();
        
        auto result = engine.cancel_order(symbol, 1);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MatchingEngine_CancelOrder);

// Benchmark: Replace orders
static void BM_MatchingEngine_ReplaceOrder(benchmark::State& state) {
    EngineConfig config;
    MatchingEngine engine(config);
    
    Symbol symbol("AAPL");
    engine.add_symbol(symbol);
    
    for (auto _ : state) {
        state.PauseTiming();
        Order order(1, symbol, Side::Buy, 10000, 100);
        engine.submit_order(order);
        state.ResumeTiming();
        
        auto result = engine.replace_order(symbol, 1, 10100, 200);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MatchingEngine_ReplaceOrder);

// Benchmark: Multiple symbols
static void BM_MatchingEngine_MultiSymbol(benchmark::State& state) {
    EngineConfig config;
    config.num_symbols = 10;
    MatchingEngine engine(config);
    
    // Add multiple symbols
    for (int i = 0; i < 10; ++i) {
        char symbol_str[9];
        snprintf(symbol_str, sizeof(symbol_str), "SYM%d", i);
        Symbol symbol(symbol_str);
        engine.add_symbol(symbol);
    }
    
    OrderId order_id = 1;
    for (auto _ : state) {
        int symbol_idx = order_id % 10;
        char symbol_str[9];
        snprintf(symbol_str, sizeof(symbol_str), "SYM%d", symbol_idx);
        Symbol symbol(symbol_str);
        
        Order order(order_id++, symbol, Side::Buy, 10000, 100);
        auto result = engine.submit_order(order);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MatchingEngine_MultiSymbol);

// Benchmark: Queue-based order submission
static void BM_MatchingEngine_EnqueueOrder(benchmark::State& state) {
    EngineConfig config;
    MatchingEngine engine(config);
    
    Symbol symbol("AAPL");
    engine.add_symbol(symbol);
    
    OrderId order_id = 1;
    for (auto _ : state) {
        Order order(order_id++, symbol, Side::Buy, 10000 + (order_id % 100), 100);
        bool success = engine.enqueue_order(order);
        benchmark::DoNotOptimize(success);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MatchingEngine_EnqueueOrder);

// Benchmark: Process queued orders
static void BM_MatchingEngine_ProcessQueue(benchmark::State& state) {
    EngineConfig config;
    MatchingEngine engine(config);
    
    Symbol symbol("AAPL");
    engine.add_symbol(symbol);
    
    for (auto _ : state) {
        state.PauseTiming();
        // Enqueue some orders
        for (int i = 0; i < 100; ++i) {
            Order order(i, symbol, Side::Buy, 10000 + i, 100);
            engine.enqueue_order(order);
        }
        state.ResumeTiming();
        
        size_t processed = engine.process_queue();
        benchmark::DoNotOptimize(processed);
    }
    
    state.SetItemsProcessed(state.iterations() * 100);
}
BENCHMARK(BM_MatchingEngine_ProcessQueue);

// Benchmark: Engine with callbacks
static void BM_MatchingEngine_WithCallbacks(benchmark::State& state) {
    EngineConfig config;
    MatchingEngine engine(config);
    
    Symbol symbol("AAPL");
    engine.add_symbol(symbol);
    
    // Set up callbacks
    int trade_count = 0;
    engine.set_trade_callback([&trade_count](const Trade& trade) {
        ++trade_count;
    });
    
    // Pre-populate with sell orders
    for (int i = 0; i < 100; ++i) {
        Order sell(i, symbol, Side::Sell, 10100, 100);
        engine.submit_order(sell);
    }
    
    OrderId order_id = 1000;
    for (auto _ : state) {
        Order buy(order_id++, symbol, Side::Buy, 10100, 50);
        auto result = engine.submit_order(buy);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MatchingEngine_WithCallbacks);

BENCHMARK_MAIN();
