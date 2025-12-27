// tests/benchmarks/matching_bench.cpp
//
// Google Benchmark tests for MatchingEngine performance
// Measures end-to-end order processing throughput
//

#include <benchmark/benchmark.h>
#include <random>
#include <vector>

#include "engine/matching_engine.hpp"
#include "core/order.hpp"

using namespace exchange;

// ============================================================================
// Fixtures
// ============================================================================

class MatchingEngineFixture : public benchmark::Fixture {
public:
    void SetUp(const benchmark::State&) override {
        engine_ = std::make_unique<MatchingEngine>();
        
        // Add symbols
        for (int i = 0; i < 10; ++i) {
            char name[9];
            snprintf(name, sizeof(name), "SYM%d", i);
            symbols_.emplace_back(name);
            engine_->add_symbol(symbols_.back());
        }
        
        next_id_ = 1;
        rng_.seed(42);
    }

    void TearDown(const benchmark::State&) override {
        engine_.reset();
        symbols_.clear();
    }

protected:
    Order make_order(const Symbol& sym, Side side, Price price, Quantity qty) {
        return Order(next_id_++, sym, side, price, qty);
    }

    const Symbol& random_symbol() {
        std::uniform_int_distribution<size_t> dist(0, symbols_.size() - 1);
        return symbols_[dist(rng_)];
    }

    std::unique_ptr<MatchingEngine> engine_;
    std::vector<Symbol> symbols_;
    OrderId next_id_ = 1;
    std::mt19937 rng_;
};

// ============================================================================
// Submit Order Benchmarks
// ============================================================================

BENCHMARK_DEFINE_F(MatchingEngineFixture, SubmitOrder_SingleSymbol)(benchmark::State& state) {
    const Symbol& sym = symbols_[0];
    bool is_bid = true;
    Price bid_price = 9000;
    Price ask_price = 11000;

    for (auto _ : state) {
        Side side = is_bid ? Side::Buy : Side::Sell;
        Price price = is_bid ? bid_price-- : ask_price++;
        
        Order order = make_order(sym, side, price, 100);
        benchmark::DoNotOptimize(engine_->submit_order(order));
        
        is_bid = !is_bid;
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(MatchingEngineFixture, SubmitOrder_SingleSymbol)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_DEFINE_F(MatchingEngineFixture, SubmitOrder_MultiSymbol)(benchmark::State& state) {
    std::uniform_int_distribution<Price> price_dist(9000, 11000);
    std::uniform_int_distribution<int> side_dist(0, 1);

    for (auto _ : state) {
        const Symbol& sym = random_symbol();
        Side side = side_dist(rng_) ? Side::Buy : Side::Sell;
        Price price = price_dist(rng_);
        
        Order order = make_order(sym, side, price, 100);
        benchmark::DoNotOptimize(engine_->submit_order(order));
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(MatchingEngineFixture, SubmitOrder_MultiSymbol)
    ->Unit(benchmark::kNanosecond);

// ============================================================================
// Queue-based Processing Benchmarks
// ============================================================================

BENCHMARK_DEFINE_F(MatchingEngineFixture, EnqueueAndProcess)(benchmark::State& state) {
    const Symbol& sym = symbols_[0];
    
    for (auto _ : state) {
        // Enqueue
        Order order = make_order(sym, Side::Buy, 10000, 100);
        engine_->enqueue_order(order);
        
        // Process
        benchmark::DoNotOptimize(engine_->process_one());
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(MatchingEngineFixture, EnqueueAndProcess)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_DEFINE_F(MatchingEngineFixture, BatchEnqueueThenProcess)(benchmark::State& state) {
    const int64_t batch_size = state.range(0);
    const Symbol& sym = symbols_[0];
    
    for (auto _ : state) {
        state.PauseTiming();
        // Reset engine for clean state
        engine_ = std::make_unique<MatchingEngine>();
        engine_->add_symbol(sym);
        next_id_ = 1;
        state.ResumeTiming();
        
        // Enqueue batch
        for (int64_t i = 0; i < batch_size; ++i) {
            Order order = make_order(sym, Side::Buy, 10000, 100);
            engine_->enqueue_order(order);
        }
        
        // Process all
        engine_->start();
        while (engine_->process_one()) {}
    }

    state.SetItemsProcessed(state.iterations() * batch_size);
}
BENCHMARK_REGISTER_F(MatchingEngineFixture, BatchEnqueueThenProcess)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(10000)
    ->Unit(benchmark::kMillisecond);

// ============================================================================
// Cancel Order Benchmarks
// ============================================================================

BENCHMARK_DEFINE_F(MatchingEngineFixture, CancelOrder)(benchmark::State& state) {
    const Symbol& sym = symbols_[0];
    
    // Pre-populate
    std::vector<OrderId> order_ids;
    for (int i = 0; i < 10000; ++i) {
        Order order = make_order(sym, Side::Buy, 9000 + (i % 1000), 100);
        engine_->submit_order(order);
        order_ids.push_back(order.id);
    }

    size_t idx = 0;
    for (auto _ : state) {
        if (idx >= order_ids.size()) {
            state.PauseTiming();
            // Repopulate
            engine_ = std::make_unique<MatchingEngine>();
            engine_->add_symbol(sym);
            order_ids.clear();
            next_id_ = 1;
            for (int i = 0; i < 10000; ++i) {
                Order order = make_order(sym, Side::Buy, 9000 + (i % 1000), 100);
                engine_->submit_order(order);
                order_ids.push_back(order.id);
            }
            idx = 0;
            state.ResumeTiming();
        }
        
        benchmark::DoNotOptimize(engine_->cancel_order(sym, order_ids[idx++]));
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(MatchingEngineFixture, CancelOrder)
    ->Unit(benchmark::kNanosecond);

// ============================================================================
// Throughput with Matching
// ============================================================================

static void BM_MatchingThroughput(benchmark::State& state) {
    const int64_t num_orders = state.range(0);
    
    for (auto _ : state) {
        MatchingEngine engine;
        Symbol sym("THRU");
        engine.add_symbol(sym);
        
        OrderId id = 1;
        int trades = 0;
        
        // Alternating buy/sell at same price = every other order matches
        for (int64_t i = 0; i < num_orders; ++i) {
            Side side = (i % 2) ? Side::Buy : Side::Sell;
            Order order(id++, sym, side, 10000, 100);
            auto result = engine.submit_order(order);
            if (result.filled_qty > 0) trades++;
        }
        
        benchmark::DoNotOptimize(trades);
    }

    state.SetItemsProcessed(state.iterations() * num_orders);
    state.counters["orders"] = benchmark::Counter(
        static_cast<double>(num_orders), 
        benchmark::Counter::kIsIterationInvariantRate
    );
}
BENCHMARK(BM_MatchingThroughput)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Unit(benchmark::kMillisecond);

// ============================================================================
// Symbol Lookup Overhead
// ============================================================================

BENCHMARK_DEFINE_F(MatchingEngineFixture, SymbolLookup_Hit)(benchmark::State& state) {
    const Symbol& sym = symbols_[0];
    
    for (auto _ : state) {
        Order order = make_order(sym, Side::Buy, 10000, 100);
        benchmark::DoNotOptimize(engine_->submit_order(order));
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(MatchingEngineFixture, SymbolLookup_Hit)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_DEFINE_F(MatchingEngineFixture, SymbolLookup_Miss)(benchmark::State& state) {
    Symbol unknown("UNKNOWN");
    
    for (auto _ : state) {
        Order order = make_order(unknown, Side::Buy, 10000, 100);
        benchmark::DoNotOptimize(engine_->submit_order(order));
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(MatchingEngineFixture, SymbolLookup_Miss)
    ->Unit(benchmark::kNanosecond);

// ============================================================================
// Statistics
// ============================================================================

BENCHMARK_DEFINE_F(MatchingEngineFixture, GetStats)(benchmark::State& state) {
    // Run some orders first
    const Symbol& sym = symbols_[0];
    for (int i = 0; i < 1000; ++i) {
        Order order = make_order(sym, (i % 2) ? Side::Buy : Side::Sell, 10000, 100);
        engine_->submit_order(order);
    }

    for (auto _ : state) {
        benchmark::DoNotOptimize(engine_->stats());
    }
}
BENCHMARK_REGISTER_F(MatchingEngineFixture, GetStats)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_MAIN();
