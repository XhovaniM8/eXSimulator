#include <catch2/catch_test_macros.hpp>
#include "core/order.hpp"

using namespace exchange;

TEST_CASE("Order: Basic construction", "[order]") {
    Symbol symbol("AAPL");
    Order order(1, symbol, Side::Buy, 10000, 100);
    
    REQUIRE(order.id == 1);
    REQUIRE(order.price == 10000);
    REQUIRE(order.quantity == 100);
    REQUIRE(order.filled_qty == 0);
    REQUIRE(order.side == Side::Buy);
    REQUIRE(order.type == OrderType::Limit);
    REQUIRE(order.status == OrderStatus::New);
}

TEST_CASE("Order: Remaining quantity", "[order]") {
    Symbol symbol("AAPL");
    Order order(1, symbol, Side::Buy, 10000, 100);
    
    REQUIRE(order.remaining() == 100);
    
    order.fill(30);
    REQUIRE(order.remaining() == 70);
    REQUIRE(order.filled_qty == 30);
}

TEST_CASE("Order: Fill order", "[order]") {
    Symbol symbol("AAPL");
    Order order(1, symbol, Side::Buy, 10000, 100);
    
    auto filled = order.fill(50);
    REQUIRE(filled == 50);
    REQUIRE(order.filled_qty == 50);
    REQUIRE(order.status == OrderStatus::PartiallyFilled);
    REQUIRE_FALSE(order.is_filled());
    
    filled = order.fill(50);
    REQUIRE(filled == 50);
    REQUIRE(order.filled_qty == 100);
    REQUIRE(order.status == OrderStatus::Filled);
    REQUIRE(order.is_filled());
}

TEST_CASE("Order: Fill more than remaining", "[order]") {
    Symbol symbol("AAPL");
    Order order(1, symbol, Side::Buy, 10000, 100);
    
    auto filled = order.fill(150);
    REQUIRE(filled == 100);  // Can only fill remaining quantity
    REQUIRE(order.filled_qty == 100);
    REQUIRE(order.is_filled());
}

TEST_CASE("Order: Cancel order", "[order]") {
    Symbol symbol("AAPL");
    Order order(1, symbol, Side::Buy, 10000, 100);
    
    order.cancel();
    REQUIRE(order.status == OrderStatus::Cancelled);
    REQUIRE_FALSE(order.is_active());
}

TEST_CASE("Order: Price priority for buy orders", "[order]") {
    Symbol symbol("AAPL");
    Order buy1(1, symbol, Side::Buy, 10100, 100);
    Order buy2(2, symbol, Side::Buy, 10000, 100);
    
    REQUIRE(buy1.has_price_priority_over(buy2));  // Higher price has priority
    REQUIRE_FALSE(buy2.has_price_priority_over(buy1));
}

TEST_CASE("Order: Price priority for sell orders", "[order]") {
    Symbol symbol("AAPL");
    Order sell1(1, symbol, Side::Sell, 10000, 100);
    Order sell2(2, symbol, Side::Sell, 10100, 100);
    
    REQUIRE(sell1.has_price_priority_over(sell2));  // Lower price has priority
    REQUIRE_FALSE(sell2.has_price_priority_over(sell1));
}

TEST_CASE("Order: Would cross buy order", "[order]") {
    Symbol symbol("AAPL");
    Order buy(1, symbol, Side::Buy, 10100, 100);
    
    REQUIRE(buy.would_cross(10100));  // Exact price
    REQUIRE(buy.would_cross(10000));  // Lower price
    REQUIRE_FALSE(buy.would_cross(10200));  // Higher price
}

TEST_CASE("Order: Would cross sell order", "[order]") {
    Symbol symbol("AAPL");
    Order sell(1, symbol, Side::Sell, 10000, 100);
    
    REQUIRE(sell.would_cross(10000));  // Exact price
    REQUIRE(sell.would_cross(10100));  // Higher price
    REQUIRE_FALSE(sell.would_cross(9900));  // Lower price
}

TEST_CASE("Order: Is active", "[order]") {
    Symbol symbol("AAPL");
    Order order(1, symbol, Side::Buy, 10000, 100);
    
    REQUIRE(order.is_active());
    
    order.fill(50);
    REQUIRE(order.is_active());  // Partially filled is still active
    
    order.fill(50);
    REQUIRE_FALSE(order.is_active());  // Fully filled is not active
}

TEST_CASE("Order: Market order", "[order]") {
    Symbol symbol("AAPL");
    Order order(1, symbol, Side::Buy, 0, 100, OrderType::Market);
    
    REQUIRE(order.type == OrderType::Market);
    REQUIRE(order.price == 0);
}

TEST_CASE("Order: IOC time in force", "[order]") {
    Symbol symbol("AAPL");
    Order order(1, symbol, Side::Buy, 10000, 100, OrderType::Limit, TimeInForce::ImmediateOrCancel);
    
    REQUIRE(order.tif == TimeInForce::ImmediateOrCancel);
}

TEST_CASE("Order: FOK time in force", "[order]") {
    Symbol symbol("AAPL");
    Order order(1, symbol, Side::Buy, 10000, 100, OrderType::Limit, TimeInForce::FillOrKill);
    
    REQUIRE(order.tif == TimeInForce::FillOrKill);
}
