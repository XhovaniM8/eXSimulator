// tests/unit/order_test.cpp
//
// Unit tests for Order struct and related types
//

#include <catch2/catch_test_macros.hpp>

#include "core/order.hpp"
#include "core/types.hpp"

using namespace exchange;

TEST_CASE("Order construction", "[order]") {
    Symbol sym("AAPL");
    Order order(1, sym, Side::Buy, 10000, 100);
    
    REQUIRE(order.id == 1);
    REQUIRE(order.side == Side::Buy);
    REQUIRE(order.price == 10000);
    REQUIRE(order.quantity == 100);
    REQUIRE(order.filled_qty == 0);
    REQUIRE(order.status == OrderStatus::New);
}

TEST_CASE("Order remaining quantity", "[order]") {
    Symbol sym("TEST");
    Order order(1, sym, Side::Buy, 10000, 100);
    
    REQUIRE(order.remaining() == 100);
    
    order.fill(30);
    REQUIRE(order.remaining() == 70);
    REQUIRE(order.filled_qty == 30);
    
    order.fill(70);
    REQUIRE(order.remaining() == 0);
    REQUIRE(order.is_filled());
}

TEST_CASE("Order fill updates status", "[order]") {
    Symbol sym("TEST");
    Order order(1, sym, Side::Buy, 10000, 100);
    
    REQUIRE(order.status == OrderStatus::New);
    
    order.fill(50);
    REQUIRE(order.status == OrderStatus::PartiallyFilled);
    
    order.fill(50);
    REQUIRE(order.status == OrderStatus::Filled);
}

TEST_CASE("Order would_cross logic", "[order]") {
    Symbol sym("TEST");
    
    SECTION("Buy order crosses at or below its price") {
        Order buy(1, sym, Side::Buy, 10000, 100);
        
        REQUIRE(buy.would_cross(9900));   // Ask below bid price - crosses
        REQUIRE(buy.would_cross(10000));  // Ask at bid price - crosses
        REQUIRE_FALSE(buy.would_cross(10100));  // Ask above bid - no cross
    }
    
    SECTION("Sell order crosses at or above its price") {
        Order sell(1, sym, Side::Sell, 10000, 100);
        
        REQUIRE(sell.would_cross(10100));  // Bid above ask price - crosses
        REQUIRE(sell.would_cross(10000));  // Bid at ask price - crosses
        REQUIRE_FALSE(sell.would_cross(9900));  // Bid below ask - no cross
    }
}

TEST_CASE("Order types", "[order]") {
    Symbol sym("TEST");
    
    SECTION("Limit order") {
        Order order(1, sym, Side::Buy, 10000, 100, OrderType::Limit);
        REQUIRE(order.type == OrderType::Limit);
    }
    
    SECTION("Market order") {
        Order order(1, sym, Side::Buy, 0, 100, OrderType::Market);
        REQUIRE(order.type == OrderType::Market);
    }
    
    SECTION("IOC order") {
        Order order(1, sym, Side::Buy, 10000, 100, OrderType::ImmediateOrCancel);
        REQUIRE(order.type == OrderType::ImmediateOrCancel);
    }
    
    SECTION("FOK order") {
        Order order(1, sym, Side::Buy, 10000, 100, OrderType::FillOrKill);
        REQUIRE(order.type == OrderType::FillOrKill);
    }
    
    SECTION("PostOnly order") {
        Order order(1, sym, Side::Buy, 10000, 100, OrderType::PostOnly);
        REQUIRE(order.type == OrderType::PostOnly);
    }
}

TEST_CASE("Symbol comparison", "[types]") {
    Symbol sym1("AAPL");
    Symbol sym2("AAPL");
    Symbol sym3("GOOGL");
    
    REQUIRE(sym1 == sym2);
    REQUIRE_FALSE(sym1 == sym3);
}

TEST_CASE("Price conversion", "[types]") {
    Price p = double_to_price(123.45);
    double d = price_to_double(p);
    
    // Should be close (within precision)
    REQUIRE(d == Catch::Approx(123.45).epsilon(0.0001));
}

TEST_CASE("Order cancel", "[order]") {
    Symbol sym("TEST");
    Order order(1, sym, Side::Buy, 10000, 100);
    
    order.cancel();
    
    REQUIRE(order.status == OrderStatus::Cancelled);
}
