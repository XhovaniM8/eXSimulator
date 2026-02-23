// tests/benchmarks/order_book_bench.cpp
//
// Google Benchmark tests for OrderBook performance
// Measures throughput and latency of core operations
//

#include <benchmark/benchmark.h>
#include <memory>
#include <random>

#include "engine/order_book.hpp"
#include "core/order.hpp"

using namespace exchange;

// ============================================================================
// Fixtures
// ============================================================================

class OrderBookFixture : public benchmark::Fixture {
public:
    void SetUp(const benchmark::State&) override {
        config_.enable_self_trade_prevention = false;
        book_ = std::make_unique<OrderBook>(symbol_, config_);
        next_id_ = 1;
        rng_.seed(42);  // Deterministic for reproducibility
    }

    void TearDown(const benchmark::State&) override {
        book_.reset();
    }

protected:
    Order make_order(Side side, Price price, Quantity qty) {
        return Order(next_id_++, symbol_, side, price, qty);
    }

    Price random_price(Price base, Price range) {
        std::uniform_int_distribution<Price> dist(base - range, base + range);
        return dist(rng_);
    }

    Quantity random_qty(Quantity min_qty, Quantity max_qty) {
        std::uniform_int_distribution<Quantity> dist(min_qty, max_qty);
        return dist(rng_);
    }

    Symbol symbol_{"BENCH"};
    OrderBookConfig config_;
    std::unique_ptr<OrderBook> book_;
    OrderId next_id_ = 1;
    std::mt19937 rng_;
};

// ============================================================================
// Add Order Benchmarks
// ============================================================================

BENCHMARK_DEFINE_F(OrderBookFixture, AddOrder_NoMatch)(benchmark::State& state) {
    // Orders that don't match (bids below asks)
    Price bid_base = 9000;
    Price ask_base = 11000;
    bool is_bid = true;

    for (auto _ : state) {
        Side side = is_bid ? Side::Buy : Side::Sell;
        Price price = is_bid ? bid_base-- : ask_base++;
        
        Order order = make_order(side, price, 100);
        benchmark::DoNotOptimize(book_->add_order(order));
        
        is_bid = !is_bid;
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(OrderBookFixture, AddOrder_NoMatch)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_DEFINE_F(OrderBookFixture, AddOrder_AlwaysMatch)(benchmark::State& state) {
    // Pre-populate with resting orders
    for (int i = 0; i < 1000; ++i) {
        book_->add_order(make_order(Side::Sell, 10000, 1000000));
    }

    for (auto _ : state) {
        Order order = make_order(Side::Buy, 10000, 1);
        benchmark::DoNotOptimize(book_->add_order(order));
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(OrderBookFixture, AddOrder_AlwaysMatch)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_DEFINE_F(OrderBookFixture, AddOrder_SamePriceLevel)(benchmark::State& state) {
    // All orders at same price - tests queue performance
    for (auto _ : state) {
        Order order = make_order(Side::Buy, 10000, 100);
        benchmark::DoNotOptimize(book_->add_order(order));
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(OrderBookFixture, AddOrder_SamePriceLevel)
    ->Unit(benchmark::kNanosecond);

// ============================================================================
// Cancel Order Benchmarks
// ============================================================================

BENCHMARK_DEFINE_F(OrderBookFixture, CancelOrder_Exists)(benchmark::State& state) {
    // Pre-populate orders
    std::vector<OrderId> order_ids;
    for (int i = 0; i < 100000; ++i) {
        Order order = make_order(Side::Buy, random_price(10000, 500), 100);
        book_->add_order(order);
        order_ids.push_back(order.id);
    }

    size_t idx = 0;
    for (auto _ : state) {
        if (idx >= order_ids.size()) {
            state.PauseTiming();
            // Repopulate
            order_ids.clear();
            book_ = std::make_unique<OrderBook>(symbol_, config_);
            next_id_ = 1;
            for (int i = 0; i < 100000; ++i) {
                Order order = make_order(Side::Buy, random_price(10000, 500), 100);
                book_->add_order(order);
                order_ids.push_back(order.id);
            }
            idx = 0;
            state.ResumeTiming();
        }
        benchmark::DoNotOptimize(book_->cancel_order(order_ids[idx++]));
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(OrderBookFixture, CancelOrder_Exists)
    ->Unit(benchmark::kNanosecond);

// ============================================================================
// Mixed Workload Benchmarks
// ============================================================================

BENCHMARK_DEFINE_F(OrderBookFixture, MixedWorkload_Realistic)(benchmark::State& state) {
    // Simulate realistic order flow:
    // - 60% new orders
    // - 30% cancels
    // - 10% aggressive (crossing) orders
    
    std::vector<OrderId> active_orders;
    std::uniform_int_distribution<int> action_dist(0, 99);
    
    // Pre-populate some orders
    for (int i = 0; i < 1000; ++i) {
        Side side = (i % 2) ? Side::Buy : Side::Sell;
        Price price = (side == Side::Buy) ? 9900 - (i / 2) : 10100 + (i / 2);
        Order order = make_order(side, price, 100);
        book_->add_order(order);
        active_orders.push_back(order.id);
    }

    for (auto _ : state) {
        int action = action_dist(rng_);
        
        if (action < 60) {
            // New passive order
            Side side = (action % 2) ? Side::Buy : Side::Sell;
            Price price = (side == Side::Buy) ? 
                random_price(9800, 100) : random_price(10200, 100);
            Order order = make_order(side, price, random_qty(1, 100));
            auto result = book_->add_order(order);
            if (result.success && result.filled_qty == 0) {
                active_orders.push_back(order.id);
            }
        } else if (action < 90 && !active_orders.empty()) {
            // Cancel
            std::uniform_int_distribution<size_t> idx_dist(0, active_orders.size() - 1);
            size_t idx = idx_dist(rng_);
            book_->cancel_order(active_orders[idx]);
            active_orders.erase(active_orders.begin() + static_cast<long>(idx));
        } else {
            // Aggressive order (crosses spread)
            Side side = (action % 2) ? Side::Buy : Side::Sell;
            Price price = (side == Side::Buy) ? 10500 : 9500;
            Order order = make_order(side, price, random_qty(1, 50));
            benchmark::DoNotOptimize(book_->add_order(order));
        }
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(OrderBookFixture, MixedWorkload_Realistic)
    ->Unit(benchmark::kNanosecond);

// ============================================================================
// Depth Retrieval Benchmarks
// ============================================================================

BENCHMARK_DEFINE_F(OrderBookFixture, GetBids_Shallow)(benchmark::State& state) {
    // Populate 100 price levels
    for (int i = 0; i < 100; ++i) {
        book_->add_order(make_order(Side::Buy, 10000 - i * 10, 100));
    }

    for (auto _ : state) {
        benchmark::DoNotOptimize(book_->get_bids(5));
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(OrderBookFixture, GetBids_Shallow)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_DEFINE_F(OrderBookFixture, GetBids_Deep)(benchmark::State& state) {
    // Populate 1000 price levels
    for (int i = 0; i < 1000; ++i) {
        book_->add_order(make_order(Side::Buy, 10000 - i, 100));
    }

    for (auto _ : state) {
        benchmark::DoNotOptimize(book_->get_bids(100));
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(OrderBookFixture, GetBids_Deep)
    ->Unit(benchmark::kNanosecond);

// ============================================================================
// Throughput Test
// ============================================================================

static void BM_Throughput_OrdersPerSecond(benchmark::State& state) {
    Symbol symbol("THRU");
    OrderBookConfig config;
    config.enable_self_trade_prevention = false;
    
    const int64_t batch_size = state.range(0);
    
    for (auto _ : state) {
        OrderBook book(symbol, config);
        OrderId id = 1;
        
        // Add batch of orders
        for (int64_t i = 0; i < batch_size; ++i) {
            Side side = (i % 2) ? Side::Buy : Side::Sell;
            Price price = (side == Side::Buy) ? 9900 : 10100;
            Order order(id++, symbol, side, price, 100);
            book.add_order(order);
        }
        
        benchmark::DoNotOptimize(book.best_bid());
    }
    
    state.SetItemsProcessed(state.iterations() * batch_size);
}
BENCHMARK(BM_Throughput_OrdersPerSecond)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Unit(benchmark::kMillisecond);

// ============================================================================
// Main
// ============================================================================

BENCHMARK_MAIN();
