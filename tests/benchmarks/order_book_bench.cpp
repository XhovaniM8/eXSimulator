#include <benchmark/benchmark.h>
#include "engine/order_book.hpp"
#include "core/order.hpp"

using namespace exchange;

// Benchmark: Add limit orders to orderbook
static void BM_OrderBook_AddLimitOrder(benchmark::State& state) {
    Symbol symbol("AAPL");
    OrderBookConfig config;
    OrderBook book(symbol, config);
    
    OrderId order_id = 1;
    for (auto _ : state) {
        Price price = static_cast<Price>(10000 + (order_id % 100));
        Order order(order_id, symbol, Side::Buy, price, 100);
        order_id++;
        auto result = book.add_order(order);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_OrderBook_AddLimitOrder);

// Benchmark: Add and immediately fill orders (matching)
static void BM_OrderBook_Matching(benchmark::State& state) {
    Symbol symbol("AAPL");
    OrderBookConfig config;
    OrderBook book(symbol, config);
    
    // Pre-populate with sell orders
    for (OrderId i = 0; i < 100; ++i) {
        Order sell(i, symbol, Side::Sell, static_cast<Price>(10100 + i), 100);
        book.add_order(sell);
    }
    
    OrderId order_id = 1000;
    for (auto _ : state) {
        Order buy(order_id++, symbol, Side::Buy, 10100, 50);
        auto result = book.add_order(buy);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_OrderBook_Matching);

// Benchmark: Cancel orders
static void BM_OrderBook_CancelOrder(benchmark::State& state) {
    Symbol symbol("AAPL");
    OrderBookConfig config;
    
    for (auto _ : state) {
        state.PauseTiming();
        OrderBook book(symbol, config);
        Order order(1, symbol, Side::Buy, 10000, 100);
        book.add_order(order);
        state.ResumeTiming();
        
        auto result = book.cancel_order(1);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_OrderBook_CancelOrder);

// Benchmark: Replace orders
static void BM_OrderBook_ReplaceOrder(benchmark::State& state) {
    Symbol symbol("AAPL");
    OrderBookConfig config;
    
    for (auto _ : state) {
        state.PauseTiming();
        OrderBook book(symbol, config);
        Order order(1, symbol, Side::Buy, 10000, 100);
        book.add_order(order);
        state.ResumeTiming();
        
        auto result = book.replace_order(1, 10100, 200);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_OrderBook_ReplaceOrder);

// Benchmark: Market data queries
static void BM_OrderBook_BestBidAsk(benchmark::State& state) {
    Symbol symbol("AAPL");
    OrderBookConfig config;
    OrderBook book(symbol, config);
    
    // Pre-populate orderbook
    for (OrderId i = 0; i < 10; ++i) {
        Order buy(i, symbol, Side::Buy, static_cast<Price>(10000 - i * 10), 100);
        Order sell(i + 100, symbol, Side::Sell, static_cast<Price>(10100 + i * 10), 100);
        book.add_order(buy);
        book.add_order(sell);
    }
    
    for (auto _ : state) {
        auto bid = book.best_bid();
        auto ask = book.best_ask();
        benchmark::DoNotOptimize(bid);
        benchmark::DoNotOptimize(ask);
    }
    
    state.SetItemsProcessed(state.iterations() * 2);
}
BENCHMARK(BM_OrderBook_BestBidAsk);

// Benchmark: Get depth (multiple price levels)
static void BM_OrderBook_GetDepth(benchmark::State& state) {
    Symbol symbol("AAPL");
    OrderBookConfig config;
    OrderBook book(symbol, config);
    
    // Pre-populate orderbook with depth
    for (OrderId i = 0; i < 50; ++i) {
        Order buy(i, symbol, Side::Buy, static_cast<Price>(10000 - i), 100);
        Order sell(i + 100, symbol, Side::Sell, static_cast<Price>(10100 + i), 100);
        book.add_order(buy);
        book.add_order(sell);
    }
    
    for (auto _ : state) {
        auto bids = book.get_bids(10);
        auto asks = book.get_asks(10);
        benchmark::DoNotOptimize(bids);
        benchmark::DoNotOptimize(asks);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_OrderBook_GetDepth);

// Benchmark: Multiple orders at same price level
static void BM_OrderBook_SamePriceLevel(benchmark::State& state) {
    Symbol symbol("AAPL");
    OrderBookConfig config;
    OrderBook book(symbol, config);
    
    OrderId order_id = 1;
    for (auto _ : state) {
        Order order(order_id++, symbol, Side::Buy, 10000, 100);
        auto result = book.add_order(order);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_OrderBook_SamePriceLevel);

// Benchmark: IOC orders
static void BM_OrderBook_IOCOrders(benchmark::State& state) {
    Symbol symbol("AAPL");
    OrderBookConfig config;
    OrderBook book(symbol, config);
    
    // Pre-populate with liquidity
    for (OrderId i = 0; i < 10; ++i) {
        Order sell(i, symbol, Side::Sell, static_cast<Price>(10100 + i), 100);
        book.add_order(sell);
    }
    
    OrderId order_id = 1000;
    for (auto _ : state) {
        Order buy(order_id++, symbol, Side::Buy, 10100, 50, OrderType::Limit, TimeInForce::ImmediateOrCancel);
        auto result = book.add_order(buy);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_OrderBook_IOCOrders);

BENCHMARK_MAIN();
