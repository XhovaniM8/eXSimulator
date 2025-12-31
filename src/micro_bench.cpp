#include <chrono>
#include <cstdio>
#include <random>

#include "engine/order_book.hpp"
#include "utils/timing.hpp"

using namespace exchange;

int main() {
    printf("OrderBook Micro-Benchmark\n");
    printf("=========================\n\n");
    
    // Setup
    Symbol symbol("BENCH");
    OrderBookConfig config;
    config.enable_self_trade_prevention = false;
    OrderBook book(symbol, config);
    
    Timing::calibrate();
    
    // Benchmark parameters
    constexpr size_t NUM_ORDERS = 10'000'000;  // 10M orders
    constexpr Price BASE_PRICE = 100'000;       // $10.00
    constexpr Quantity QTY = 100;
    
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<Price> price_dist(-1000, 1000);  // ±$0.10
    std::uniform_int_distribution<int> side_dist(0, 1);
    
    printf("Generating %zu orders...\n", NUM_ORDERS);
    std::vector<Order> orders;
    orders.reserve(NUM_ORDERS);
    
    for (size_t i = 0; i < NUM_ORDERS; ++i) {
        Order order;
        order.id = i + 1;
        order.symbol = symbol;
        order.side = side_dist(rng) ? Side::Buy : Side::Sell;
        order.price = BASE_PRICE + price_dist(rng);
        order.quantity = QTY;
        order.type = OrderType::Limit;
        order.tif = TimeInForce::Day;
        order.status = OrderStatus::New;
        orders.push_back(order);
    }
    
    printf("Running benchmark...\n");
    auto start = std::chrono::high_resolution_clock::now();
    uint64_t start_cycles = Timing::rdtsc();
    
    size_t accepted = 0;
    size_t rejected = 0;
    
    for (auto& order : orders) {
        auto result = book.add_order(order);
        if (result.success) {
            ++accepted;
        } else {
            ++rejected;
        }
    }
    
    uint64_t end_cycles = Timing::rdtsc();
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    auto duration_ms = duration_ns / 1'000'000;
    uint64_t cycles = end_cycles - start_cycles;
    
    // Results
    printf("\nResults\n");
    printf("-------\n");
    printf("Orders:        %zu\n", NUM_ORDERS);
    printf("Accepted:      %zu\n", accepted);
    printf("Rejected:      %zu\n", rejected);
    printf("Duration:      %ld ms\n", duration_ms);
    printf("Throughput:    %.2f M orders/sec\n", 
           NUM_ORDERS / (duration_ms / 1000.0) / 1'000'000.0);
    printf("Latency:       %.0f ns/order\n", 
           static_cast<double>(duration_ns) / NUM_ORDERS);
    printf("Cycles/order:  %.0f\n", 
           static_cast<double>(cycles) / NUM_ORDERS);
    
    // Book stats
    printf("\nBook State\n");
    printf("----------\n");
    printf("Orders in book: %zu\n", book.order_count());
    printf("Bid levels:     %zu\n", book.bid_level_count());
    printf("Ask levels:     %zu\n", book.ask_level_count());
    printf("Best bid:       %ld\n", book.best_bid());
    printf("Best ask:       %ld\n", book.best_ask());
    printf("Spread:         %ld\n", book.spread());
    
    return 0;
}
