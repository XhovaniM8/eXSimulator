#pragma once

#include <array>
#include <functional>
#include <unordered_map>
#include <vector>

#include "core/order.hpp"
#include "core/trade.hpp"
#include "engine/price_level.hpp"

namespace exchange {

// Callback types for order book events
using TradeCallback = std::function<void(const Trade&)>;
using ExecutionCallback = std::function<void(const ExecutionReport&)>;
using BBOCallback = std::function<void(const BBOUpdate&)>;

// Configuration for order book behavior
struct OrderBookConfig {
    size_t max_price_levels = 10000;    // Max distinct price levels per side
    size_t max_orders_per_level = 1000; // Max orders at single price
    bool enable_self_trade_prevention = false;
    bool enable_post_only_rejection = true;
    bool batch_callbacks = true;         // Batch trades for performance
    size_t trade_batch_size = 100;       // Flush trades after N trades
};

// Result of an order operation
struct OrderResult {
    bool success;
    RejectReason reject_reason;
    OrderId order_id;
    Quantity filled_qty;
    Price avg_fill_price;
};

// Order book for a single symbol
// Uses price-level based structure for O(1) best price access
class OrderBook {
public:
    explicit OrderBook(Symbol symbol, const OrderBookConfig& config = {});
    ~OrderBook();
    
    // Non-copyable, non-movable (contains internal state)
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) = delete;
    OrderBook& operator=(OrderBook&&) = delete;
    
    // Core operations
    OrderResult add_order(Order order);
    OrderResult cancel_order(OrderId order_id);
    OrderResult replace_order(OrderId order_id, Price new_price, Quantity new_qty);
    
    // Market data queries
    Price best_bid() const;
    Price best_ask() const;
    Quantity bid_quantity_at(Price price) const;
    Quantity ask_quantity_at(Price price) const;
    
    // Get top N price levels
    std::vector<PriceLevelUpdate> get_bids(size_t depth) const;
    std::vector<PriceLevelUpdate> get_asks(size_t depth) const;
    
    // Spread calculation
    Price spread() const;
    Price mid_price() const;
    
    // Statistics
    size_t order_count() const { return orders_.size(); }
    size_t bid_level_count() const;
    size_t ask_level_count() const;
    
    // Register callbacks
    void set_trade_callback(TradeCallback cb) { on_trade_ = std::move(cb); }
    void set_execution_callback(ExecutionCallback cb) { on_execution_ = std::move(cb); }
    void set_bbo_callback(BBOCallback cb) { on_bbo_ = std::move(cb); }
    
    // Flush pending batched trades (call at end of processing batch)
    void flush_pending() { flush_trades(); }
    
    // Symbol getter
    const Symbol& symbol() const { return symbol_; }
    
private:
    // Match incoming order against resting orders
    void match_order(Order& order);
    
    // Insert order into book after matching
    void insert_resting_order(Order order);
    
    // Remove order from price level
    void remove_order_from_level(OrderId order_id, Side side, Price price);
    
    // Update best prices after order changes
    void update_best_bid();
    void update_best_ask();
    
    // Emit trade event
    void emit_trade(const Order& buy, const Order& sell, 
                    Price price, Quantity qty, Side aggressor);
    
    // Emit BBO update if changed
    void maybe_emit_bbo();
    
    // Emit execution report
    void emit_execution_report(const Order& order, Quantity filled, Price avg_price);
    
    // Flush pending trades to callback
    void flush_trades();
    
    Symbol symbol_;
    OrderBookConfig config_;
    
    // Order storage: order_id -> Order
    std::unordered_map<OrderId, Order> orders_;
    
    // Price levels indexed by price
    // Pre-allocated capacity for reduced rehashing
    std::unordered_map<Price, PriceLevel> bid_levels_;
    std::unordered_map<Price, PriceLevel> ask_levels_;
    
    // Cached best prices for O(1) access
    Price best_bid_;
    Price best_ask_;
    
    // Sequence number for trades
    SequenceNum next_seq_num_;
    
    // Event callbacks
    TradeCallback on_trade_;
    ExecutionCallback on_execution_;
    BBOCallback on_bbo_;
    
    // Last emitted BBO for change detection
    Price last_bbo_bid_;
    Price last_bbo_ask_;
    
    // Trade batching for performance
    std::vector<Trade> pending_trades_;
};

}  // namespace exchange
