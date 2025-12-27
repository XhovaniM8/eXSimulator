#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

#include "engine/order_book.hpp"
#include "core/order.hpp"

namespace exchange {
namespace test {

class OrderBookTester {
public:
    OrderBookTester() : symbol_("TEST"), test_count_(0), pass_count_(0) {
        book_ = std::make_unique<OrderBook>(symbol_, config_);
    }
    
    void run_all_tests() {
        std::cout << "\n=== OrderBook Unit Tests ===" << std::endl;
        
        test_basic_limit_order();
        test_partial_fill();
        test_complete_fill();
        test_cancel_order();
        test_replace_order_same_price();
        test_replace_order_different_price();
        test_market_order_buy();
        test_market_order_sell();
        test_post_only_rejection();
        test_ioc_order();
        test_fok_order_success();
        test_fok_order_fail();
        test_self_trade_prevention();
        test_price_time_priority();
        test_multiple_price_levels();
        test_bbo_updates();
        
        std::cout << "\n=== Test Summary ===" << std::endl;
        std::cout << "Passed: " << pass_count_ << "/" << test_count_ << std::endl;
        if (pass_count_ == test_count_) {
            std::cout << "✓ All tests passed!" << std::endl;
        } else {
            std::cout << "✗ " << (test_count_ - pass_count_) << " tests failed" << std::endl;
        }
    }
    
private:
    Symbol symbol_;
    OrderBookConfig config_;
    std::unique_ptr<OrderBook> book_;
    int test_count_;
    int pass_count_;
    
    void reset_book() {
        book_ = std::make_unique<OrderBook>(symbol_, config_);
    }
    
    void check(bool condition, const std::string& test_name) {
        ++test_count_;
        if (condition) {
            ++pass_count_;
            std::cout << "✓ " << test_name << std::endl;
        } else {
            std::cout << "✗ " << test_name << " FAILED" << std::endl;
        }
    }
    
    void test_basic_limit_order() {
        reset_book();
        Symbol symbol("AAPL");
        Order order(1, symbol, Side::Buy, 10000, 100);  // $1.00, qty 100
        
        auto result = book_->add_order(order);
        check(result.success, "Basic limit order insertion");
        check(book_->best_bid() == 10000, "Best bid updated");
        check(book_->bid_quantity_at(10000) == 100, "Bid quantity correct");
    }
    
    void test_partial_fill() {
        reset_book();
        Symbol symbol("AAPL");
        
        // Add resting sell order
        Order sell(1, symbol, Side::Sell, 10100, 100);
        book_->add_order(sell);
        
        // Add buy order that partially fills
        Order buy(2, symbol, Side::Buy, 10100, 50);
        auto result = book_->add_order(buy);
        
        check(result.success, "Partial fill order accepted");
        check(book_->ask_quantity_at(10100) == 50, "Remaining quantity correct");
    }
    
    void test_complete_fill() {
        reset_book();
        Symbol symbol("AAPL");
        
        // Add resting sell
        Order sell(1, symbol, Side::Sell, 10100, 100);
        book_->add_order(sell);
        
        // Add buy that completely fills
        Order buy(2, symbol, Side::Buy, 10100, 100);
        book_->add_order(buy);
        
        check(book_->ask_quantity_at(10100) == 0, "Complete fill removes level");
        check(book_->best_ask() == INVALID_PRICE, "No remaining asks");
    }
    
    void test_cancel_order() {
        reset_book();
        Symbol symbol("AAPL");
        
        Order order(1, symbol, Side::Buy, 10000, 100);
        book_->add_order(order);
        
        auto result = book_->cancel_order(1);
        check(result.success, "Order cancellation");
        check(book_->bid_quantity_at(10000) == 0, "Cancelled order removed");
    }
    
    void test_replace_order_same_price() {
        reset_book();
        Symbol symbol("AAPL");
        
        Order order(1, symbol, Side::Buy, 10000, 100);
        book_->add_order(order);
        
        auto result = book_->replace_order(1, 10000, 200);
        check(result.success, "Replace order same price");
        check(book_->bid_quantity_at(10000) == 200, "Quantity updated");
    }
    
    void test_replace_order_different_price() {
        reset_book();
        Symbol symbol("AAPL");
        
        Order order(1, symbol, Side::Buy, 10000, 100);
        book_->add_order(order);
        
        auto result = book_->replace_order(1, 10100, 150);
        check(result.success, "Replace order different price");
        check(book_->bid_quantity_at(10000) == 0, "Old price level cleared");
        check(book_->bid_quantity_at(10100) == 150, "New price level created");
    }
    
    void test_market_order_buy() {
        reset_book();
        Symbol symbol("AAPL");
        
        // Add resting sell
        Order sell(1, symbol, Side::Sell, 10100, 100);
        book_->add_order(sell);
        
        // Market buy
        Order buy(2, symbol, Side::Buy, 0, 50, OrderType::Market);
        auto result = book_->add_order(buy);
        
        check(result.success, "Market buy order");
        check(book_->ask_quantity_at(10100) == 50, "Market order filled against ask");
    }
    
    void test_market_order_sell() {
        reset_book();
        Symbol symbol("AAPL");
        
        // Add resting buy
        Order buy(1, symbol, Side::Buy, 10000, 100);
        book_->add_order(buy);
        
        // Market sell
        Order sell(2, symbol, Side::Sell, 0, 50, OrderType::Market);
        auto result = book_->add_order(sell);
        
        check(result.success, "Market sell order");
        check(book_->bid_quantity_at(10000) == 50, "Market order filled against bid");
    }
    
    void test_post_only_rejection() {
        reset_book();
        config_.enable_post_only_rejection = true;
        book_ = std::make_unique<OrderBook>(symbol_, config_);
        
        Symbol symbol("AAPL");
        
        // Add resting sell
        Order sell(1, symbol, Side::Sell, 10100, 100);
        book_->add_order(sell);
        
        // PostOnly buy that would cross (using Day TIF, checking with PostOnly flag in book)
        Order buy(2, symbol, Side::Buy, 10100, 50, OrderType::PostOnly);
        auto result = book_->add_order(buy);
        
        check(!result.success, "PostOnly order rejected when crossing");
    }
    
    void test_ioc_order() {
        reset_book();
        Symbol symbol("AAPL");
        
        // Add partial liquidity
        Order sell(1, symbol, Side::Sell, 10100, 50);
        book_->add_order(sell);
        
        // IOC order for more than available
        Order buy(2, symbol, Side::Buy, 10100, 100, OrderType::Limit, TimeInForce::ImmediateOrCancel);
        auto result = book_->add_order(buy);
        
        check(result.success, "IOC order accepted");
        check(book_->ask_quantity_at(10100) == 0, "IOC filled available qty");
        check(book_->bid_quantity_at(10100) == 0, "IOC remainder cancelled (not resting)");
    }
    
    void test_fok_order_success() {
        reset_book();
        Symbol symbol("AAPL");
        
        // Add sufficient liquidity
        Order sell(1, symbol, Side::Sell, 10100, 100);
        book_->add_order(sell);
        
        // FOK order that can be filled
        Order buy(2, symbol, Side::Buy, 10100, 100, OrderType::Limit, TimeInForce::FillOrKill);
        auto result = book_->add_order(buy);
        
        check(result.success, "FOK order filled completely");
        check(book_->ask_quantity_at(10100) == 0, "FOK consumed all liquidity");
    }
    
    void test_fok_order_fail() {
        reset_book();
        Symbol symbol("AAPL");
        
        // Add insufficient liquidity
        Order sell(1, symbol, Side::Sell, 10100, 50);
        book_->add_order(sell);
        
        // FOK order that cannot be filled
        Order buy(2, symbol, Side::Buy, 10100, 100, OrderType::Limit, TimeInForce::FillOrKill);
        auto result = book_->add_order(buy);
        
        check(!result.success, "FOK order rejected (insufficient liquidity)");
        check(book_->ask_quantity_at(10100) == 50, "FOK rejection left book unchanged");
    }
    
    void test_self_trade_prevention() {
        reset_book();
        config_.enable_self_trade_prevention = true;
        book_ = std::make_unique<OrderBook>(symbol_, config_);
        
        Symbol symbol("AAPL");
        
        // Add resting buy (order ID 100)
        Order buy(100, symbol, Side::Buy, 10100, 50);
        book_->add_order(buy);
        
        // Add sell from same trader (order ID 101, close ID suggests same trader)
        Order sell(101, symbol, Side::Sell, 10100, 50);
        auto result = book_->add_order(sell);
        
        check(result.success, "Self-trade prevention order accepted");
        check(book_->bid_quantity_at(10100) == 50, "Self-trade prevented (buy still resting)");
        check(book_->ask_quantity_at(10100) == 50, "Self-trade prevented (sell resting)");
    }
    
    void test_price_time_priority() {
        reset_book();
        Symbol symbol("AAPL");
        
        // Add three orders at same price
        Order order1(1, symbol, Side::Buy, 10000, 100);
        Order order2(2, symbol, Side::Buy, 10000, 100);
        Order order3(3, symbol, Side::Buy, 10000, 100);
        
        book_->add_order(order1);
        book_->add_order(order2);
        book_->add_order(order3);
        
        check(book_->bid_quantity_at(10000) == 300, "All orders at same price level");
        
        // Add sell that fills first order only
        Order sell(4, symbol, Side::Sell, 10000, 100);
        book_->add_order(sell);
        
        check(book_->bid_quantity_at(10000) == 200, "FIFO: first order filled first");
    }
    
    void test_multiple_price_levels() {
        reset_book();
        Symbol symbol("AAPL");
        
        // Add orders at different prices
        Order buy1(1, symbol, Side::Buy, 10000, 100);
        Order buy2(2, symbol, Side::Buy, 9900, 100);
        Order buy3(3, symbol, Side::Buy, 9800, 100);
        
        book_->add_order(buy1);
        book_->add_order(buy2);
        book_->add_order(buy3);
        
        check(book_->best_bid() == 10000, "Best bid is highest price");
        check(book_->bid_quantity_at(10000) == 100, "Top level quantity");
        check(book_->bid_quantity_at(9900) == 100, "Second level quantity");
        check(book_->bid_quantity_at(9800) == 100, "Third level quantity");
    }
    
    void test_bbo_updates() {
        reset_book();
        Symbol symbol("AAPL");
        
        // Add bid and ask
        Order buy(1, symbol, Side::Buy, 10000, 100);
        Order sell(2, symbol, Side::Sell, 10100, 100);
        
        book_->add_order(buy);
        book_->add_order(sell);
        
        check(book_->best_bid() == 10000, "BBO: best bid");
        check(book_->best_ask() == 10100, "BBO: best ask");
        
        // Remove best bid
        book_->cancel_order(1);
        check(book_->best_bid() == INVALID_PRICE, "BBO: bid removed");
        
        // Remove best ask
        book_->cancel_order(2);
        check(book_->best_ask() == INVALID_PRICE, "BBO: ask removed");
    }
};

}  // namespace test
}  // namespace exchange

int main() {
    exchange::test::OrderBookTester tester;
    tester.run_all_tests();
    return 0;
}
