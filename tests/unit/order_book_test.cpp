#include <catch2/catch_test_macros.hpp>
#include "engine/order_book.hpp"
#include "core/order.hpp"

using namespace exchange;

TEST_CASE("OrderBook: Basic limit order insertion", "[orderbook]") {
    Symbol symbol("AAPL");
    OrderBookConfig config;
    OrderBook book(symbol, config);
    
    Order order(1, symbol, Side::Buy, 10000, 100);  // $1.00, qty 100
    auto result = book.add_order(order);
    
    REQUIRE(result.success);
    REQUIRE(book.best_bid() == 10000);
    REQUIRE(book.bid_quantity_at(10000) == 100);
}

TEST_CASE("OrderBook: Partial fill", "[orderbook]") {
    Symbol symbol("AAPL");
    OrderBookConfig config;
    OrderBook book(symbol, config);
    
    // Add resting sell order
    Order sell(1, symbol, Side::Sell, 10100, 100);
    book.add_order(sell);
    
    // Add buy order that partially fills
    Order buy(2, symbol, Side::Buy, 10100, 50);
    auto result = book.add_order(buy);
    
    REQUIRE(result.success);
    REQUIRE(book.ask_quantity_at(10100) == 50);
}

TEST_CASE("OrderBook: Complete fill", "[orderbook]") {
    Symbol symbol("AAPL");
    OrderBookConfig config;
    OrderBook book(symbol, config);
    
    // Add resting sell
    Order sell(1, symbol, Side::Sell, 10100, 100);
    book.add_order(sell);
    
    // Add buy that completely fills
    Order buy(2, symbol, Side::Buy, 10100, 100);
    book.add_order(buy);
    
    REQUIRE(book.ask_quantity_at(10100) == 0);
    REQUIRE(book.best_ask() == INVALID_PRICE);
}

TEST_CASE("OrderBook: Cancel order", "[orderbook]") {
    Symbol symbol("AAPL");
    OrderBookConfig config;
    OrderBook book(symbol, config);
    
    Order order(1, symbol, Side::Buy, 10000, 100);
    book.add_order(order);
    
    auto result = book.cancel_order(1);
    REQUIRE(result.success);
    REQUIRE(book.bid_quantity_at(10000) == 0);
}

TEST_CASE("OrderBook: Replace order same price", "[orderbook]") {
    Symbol symbol("AAPL");
    OrderBookConfig config;
    OrderBook book(symbol, config);
    
    Order order(1, symbol, Side::Buy, 10000, 100);
    book.add_order(order);
    
    auto result = book.replace_order(1, 10000, 200);
    REQUIRE(result.success);
    REQUIRE(book.bid_quantity_at(10000) == 200);
}

TEST_CASE("OrderBook: Replace order different price", "[orderbook]") {
    Symbol symbol("AAPL");
    OrderBookConfig config;
    OrderBook book(symbol, config);
    
    Order order(1, symbol, Side::Buy, 10000, 100);
    book.add_order(order);
    
    auto result = book.replace_order(1, 10100, 150);
    REQUIRE(result.success);
    REQUIRE(book.bid_quantity_at(10000) == 0);
    REQUIRE(book.bid_quantity_at(10100) == 150);
}

TEST_CASE("OrderBook: Market order buy", "[orderbook]") {
    Symbol symbol("AAPL");
    OrderBookConfig config;
    OrderBook book(symbol, config);
    
    // Add resting sell
    Order sell(1, symbol, Side::Sell, 10100, 100);
    book.add_order(sell);
    
    // Market buy
    Order buy(2, symbol, Side::Buy, 0, 50, OrderType::Market);
    auto result = book.add_order(buy);
    
    REQUIRE(result.success);
    REQUIRE(book.ask_quantity_at(10100) == 50);
}

TEST_CASE("OrderBook: Market order sell", "[orderbook]") {
    Symbol symbol("AAPL");
    OrderBookConfig config;
    OrderBook book(symbol, config);
    
    // Add resting buy
    Order buy(1, symbol, Side::Buy, 10000, 100);
    book.add_order(buy);
    
    // Market sell
    Order sell(2, symbol, Side::Sell, 0, 50, OrderType::Market);
    auto result = book.add_order(sell);
    
    REQUIRE(result.success);
    REQUIRE(book.bid_quantity_at(10000) == 50);
}

TEST_CASE("OrderBook: IOC order", "[orderbook]") {
    Symbol symbol("AAPL");
    OrderBookConfig config;
    OrderBook book(symbol, config);
    
    // Add partial liquidity
    Order sell(1, symbol, Side::Sell, 10100, 50);
    book.add_order(sell);
    
    // IOC order for more than available
    Order buy(2, symbol, Side::Buy, 10100, 100, OrderType::Limit, TimeInForce::ImmediateOrCancel);
    auto result = book.add_order(buy);
    
    REQUIRE(result.success);
    REQUIRE(book.ask_quantity_at(10100) == 0);
    REQUIRE(book.bid_quantity_at(10100) == 0);
}

TEST_CASE("OrderBook: FOK order success", "[orderbook]") {
    Symbol symbol("AAPL");
    OrderBookConfig config;
    OrderBook book(symbol, config);
    
    // Add sufficient liquidity
    Order sell(1, symbol, Side::Sell, 10100, 100);
    book.add_order(sell);
    
    // FOK order that can be filled
    Order buy(2, symbol, Side::Buy, 10100, 100, OrderType::Limit, TimeInForce::FillOrKill);
    auto result = book.add_order(buy);
    
    REQUIRE(result.success);
    REQUIRE(book.ask_quantity_at(10100) == 0);
}

TEST_CASE("OrderBook: FOK order fail", "[orderbook]") {
    Symbol symbol("AAPL");
    OrderBookConfig config;
    OrderBook book(symbol, config);
    
    // Add insufficient liquidity
    Order sell(1, symbol, Side::Sell, 10100, 50);
    book.add_order(sell);
    
    // FOK order that cannot be filled
    Order buy(2, symbol, Side::Buy, 10100, 100, OrderType::Limit, TimeInForce::FillOrKill);
    auto result = book.add_order(buy);
    
    REQUIRE_FALSE(result.success);
    REQUIRE(book.ask_quantity_at(10100) == 50);
}

TEST_CASE("OrderBook: Price-time priority", "[orderbook]") {
    Symbol symbol("AAPL");
    OrderBookConfig config;
    OrderBook book(symbol, config);
    
    // Add three orders at same price
    Order order1(1, symbol, Side::Buy, 10000, 100);
    Order order2(2, symbol, Side::Buy, 10000, 100);
    Order order3(3, symbol, Side::Buy, 10000, 100);
    
    book.add_order(order1);
    book.add_order(order2);
    book.add_order(order3);
    
    REQUIRE(book.bid_quantity_at(10000) == 300);
    
    // Add sell that fills first order only
    Order sell(4, symbol, Side::Sell, 10000, 100);
    book.add_order(sell);
    
    REQUIRE(book.bid_quantity_at(10000) == 200);
}

TEST_CASE("OrderBook: Multiple price levels", "[orderbook]") {
    Symbol symbol("AAPL");
    OrderBookConfig config;
    OrderBook book(symbol, config);
    
    // Add orders at different prices
    Order buy1(1, symbol, Side::Buy, 10000, 100);
    Order buy2(2, symbol, Side::Buy, 9900, 100);
    Order buy3(3, symbol, Side::Buy, 9800, 100);
    
    book.add_order(buy1);
    book.add_order(buy2);
    book.add_order(buy3);
    
    REQUIRE(book.best_bid() == 10000);
    REQUIRE(book.bid_quantity_at(10000) == 100);
    REQUIRE(book.bid_quantity_at(9900) == 100);
    REQUIRE(book.bid_quantity_at(9800) == 100);
}

TEST_CASE("OrderBook: BBO updates", "[orderbook]") {
    Symbol symbol("AAPL");
    OrderBookConfig config;
    OrderBook book(symbol, config);
    
    // Add bid and ask
    Order buy(1, symbol, Side::Buy, 10000, 100);
    Order sell(2, symbol, Side::Sell, 10100, 100);
    
    book.add_order(buy);
    book.add_order(sell);
    
    REQUIRE(book.best_bid() == 10000);
    REQUIRE(book.best_ask() == 10100);
    
    // Remove best bid
    book.cancel_order(1);
    REQUIRE(book.best_bid() == INVALID_PRICE);
    
    // Remove best ask
    book.cancel_order(2);
    REQUIRE(book.best_ask() == INVALID_PRICE);
}

TEST_CASE("OrderBook: Spread calculation", "[orderbook]") {
    Symbol symbol("AAPL");
    OrderBookConfig config;
    OrderBook book(symbol, config);
    
    Order buy(1, symbol, Side::Buy, 10000, 100);
    Order sell(2, symbol, Side::Sell, 10100, 100);
    
    book.add_order(buy);
    book.add_order(sell);
    
    REQUIRE(book.spread() == 100);
    REQUIRE(book.mid_price() == 10050);
}
