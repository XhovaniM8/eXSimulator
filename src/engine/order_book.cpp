#include "engine/order_book.hpp"

#include <algorithm>

namespace exchange {

OrderBook::OrderBook(Symbol symbol, const OrderBookConfig& config)
    : symbol_(symbol)
    , config_(config)
    , best_bid_(0)
    , best_ask_(INVALID_PRICE)
    , next_seq_num_(1)
    , last_bbo_bid_(0)
    , last_bbo_ask_(INVALID_PRICE)
{
}

OrderBook::~OrderBook() = default;

OrderResult OrderBook::add_order(Order order) {
    OrderResult result{};
    result.order_id = order.id;
    
    // Validate
    if (order.quantity == 0) {
        result.success = false;
        result.reject_reason = RejectReason::InvalidQuantity;
        return result;
    }
    
    if (order.type == OrderType::Limit && order.price <= 0) {
        result.success = false;
        result.reject_reason = RejectReason::InvalidPrice;
        return result;
    }
    
    // Check for duplicate
    if (orders_.count(order.id)) {
        result.success = false;
        result.reject_reason = RejectReason::DuplicateOrderId;
        return result;
    }
    
    // Match against resting orders
    match_order(order);
    
    result.filled_qty = order.filled_qty;
    
    // If order has remaining quantity and is not IOC/FOK, add to book
    if (!order.is_filled() && order.type == OrderType::Limit) {
        insert_resting_order(order);
    }
    
    result.success = true;
    return result;
}

OrderResult OrderBook::cancel_order(OrderId order_id) {
    OrderResult result{};
    result.order_id = order_id;
    
    auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        result.success = false;
        result.reject_reason = RejectReason::OrderNotFound;
        return result;
    }
    
    Order& order = it->second;
    remove_order_from_level(order_id, order.side, order.price);
    order.cancel();
    orders_.erase(it);
    
    // Update best prices
    if (order.side == Side::Buy) {
        update_best_bid();
    } else {
        update_best_ask();
    }
    
    maybe_emit_bbo();
    
    result.success = true;
    return result;
}

OrderResult OrderBook::replace_order(OrderId order_id, Price new_price, Quantity new_qty) {
    // Cancel and re-add
    auto cancel_result = cancel_order(order_id);
    if (!cancel_result.success) {
        return cancel_result;
    }
    
    // TODO: Implement proper replace logic
    // For now, just return cancel result
    return cancel_result;
}

void OrderBook::match_order(Order& order) {
    auto& opposite_levels = (order.side == Side::Buy) ? ask_levels_ : bid_levels_;
    
    while (!order.is_filled()) {
        // Find best opposing price
        Price best_opposite = (order.side == Side::Buy) ? best_ask_ : best_bid_;
        
        if (best_opposite == INVALID_PRICE || best_opposite == 0) {
            break;  // No liquidity
        }
        
        // Check if order can match
        if (!order.would_cross(best_opposite)) {
            break;  // Price doesn't cross
        }
        
        // Get level at best price
        auto level_it = opposite_levels.find(best_opposite);
        if (level_it == opposite_levels.end() || level_it->second.empty()) {
            // Update best price and try again
            if (order.side == Side::Buy) {
                update_best_ask();
            } else {
                update_best_bid();
            }
            continue;
        }
        
        PriceLevel& level = level_it->second;
        
        // Match against orders at this level
        while (!order.is_filled() && !level.empty()) {
            OrderId resting_id = level.front_order_id();
            Quantity resting_qty = level.front_quantity();
            
            auto resting_it = orders_.find(resting_id);
            if (resting_it == orders_.end()) {
                level.pop_front();
                continue;
            }
            
            Order& resting = resting_it->second;
            Quantity fill_qty = std::min(order.remaining(), resting.remaining());
            
            // Execute trade
            order.fill(fill_qty);
            resting.fill(fill_qty);
            level.fill_front(fill_qty);
            
            // Emit trade
            if (order.side == Side::Buy) {
                emit_trade(order, resting, best_opposite, fill_qty, Side::Buy);
            } else {
                emit_trade(resting, order, best_opposite, fill_qty, Side::Sell);
            }
            
            // Remove filled resting order
            if (resting.is_filled()) {
                level.pop_front();
                orders_.erase(resting_it);
            }
        }
        
        // Clean up empty level
        if (level.empty()) {
            opposite_levels.erase(level_it);
            if (order.side == Side::Buy) {
                update_best_ask();
            } else {
                update_best_bid();
            }
        }
    }
}

void OrderBook::insert_resting_order(Order order) {
    auto& levels = (order.side == Side::Buy) ? bid_levels_ : ask_levels_;
    
    // Get or create price level
    auto [it, inserted] = levels.try_emplace(order.price, order.price);
    it->second.add_order(order.id, order.remaining());
    
    // Store order
    orders_[order.id] = order;
    
    // Update best price
    if (order.side == Side::Buy) {
        if (order.price > best_bid_) {
            best_bid_ = order.price;
        }
    } else {
        if (order.price < best_ask_) {
            best_ask_ = order.price;
        }
    }
    
    maybe_emit_bbo();
}

void OrderBook::remove_order_from_level(OrderId order_id, Side side, Price price) {
    auto& levels = (side == Side::Buy) ? bid_levels_ : ask_levels_;
    
    auto it = levels.find(price);
    if (it != levels.end()) {
        it->second.remove_order(order_id);
        if (it->second.empty()) {
            levels.erase(it);
        }
    }
}

void OrderBook::update_best_bid() {
    best_bid_ = 0;
    for (const auto& [price, level] : bid_levels_) {
        if (!level.empty() && price > best_bid_) {
            best_bid_ = price;
        }
    }
}

void OrderBook::update_best_ask() {
    best_ask_ = INVALID_PRICE;
    for (const auto& [price, level] : ask_levels_) {
        if (!level.empty() && price < best_ask_) {
            best_ask_ = price;
        }
    }
}

void OrderBook::emit_trade(const Order& buy, const Order& sell,
                           Price price, Quantity qty, Side aggressor) {
    if (!on_trade_) return;
    
    Trade trade;
    trade.seq_num = next_seq_num_++;
    trade.symbol = symbol_;
    trade.price = price;
    trade.quantity = qty;
    trade.buy_order_id = buy.id;
    trade.sell_order_id = sell.id;
    trade.timestamp = Timing::now_ns();
    trade.aggressor_side = aggressor;
    
    on_trade_(trade);
}

void OrderBook::maybe_emit_bbo() {
    if (!on_bbo_) return;
    
    if (best_bid_ != last_bbo_bid_ || best_ask_ != last_bbo_ask_) {
        BBOUpdate bbo;
        bbo.symbol = symbol_;
        bbo.bid_price = best_bid_;
        bbo.bid_qty = bid_quantity_at(best_bid_);
        bbo.ask_price = best_ask_;
        bbo.ask_qty = ask_quantity_at(best_ask_);
        bbo.timestamp = Timing::now_ns();
        
        on_bbo_(bbo);
        
        last_bbo_bid_ = best_bid_;
        last_bbo_ask_ = best_ask_;
    }
}

Price OrderBook::best_bid() const { return best_bid_; }
Price OrderBook::best_ask() const { return best_ask_; }

Quantity OrderBook::bid_quantity_at(Price price) const {
    auto it = bid_levels_.find(price);
    return (it != bid_levels_.end()) ? it->second.total_quantity() : 0;
}

Quantity OrderBook::ask_quantity_at(Price price) const {
    auto it = ask_levels_.find(price);
    return (it != ask_levels_.end()) ? it->second.total_quantity() : 0;
}

Price OrderBook::spread() const {
    if (best_ask_ == INVALID_PRICE || best_bid_ == 0) {
        return INVALID_PRICE;
    }
    return best_ask_ - best_bid_;
}

Price OrderBook::mid_price() const {
    if (best_ask_ == INVALID_PRICE || best_bid_ == 0) {
        return 0;
    }
    return (best_bid_ + best_ask_) / 2;
}

size_t OrderBook::bid_level_count() const { return bid_levels_.size(); }
size_t OrderBook::ask_level_count() const { return ask_levels_.size(); }

std::vector<PriceLevelUpdate> OrderBook::get_bids(size_t depth) const {
    std::vector<PriceLevelUpdate> result;
    // TODO: Sort by price descending
    return result;
}

std::vector<PriceLevelUpdate> OrderBook::get_asks(size_t depth) const {
    std::vector<PriceLevelUpdate> result;
    // TODO: Sort by price ascending
    return result;
}

}  // namespace exchange
