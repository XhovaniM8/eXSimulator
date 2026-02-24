#include "engine/order_book.hpp"

#include "core/types.hpp"
#include "utils/timing.hpp"
#include <algorithm>

namespace exchange {

OrderBook::OrderBook(Symbol symbol, const OrderBookConfig &config)
    : symbol_(symbol), config_(config), best_bid_(INVALID_PRICE),
      best_ask_(INVALID_PRICE), next_seq_num_(1), last_bbo_bid_(INVALID_PRICE),
      last_bbo_ask_(INVALID_PRICE) {
  // Reserve hash map capacity to avoid rehashing in hot path
  orders_.reserve(10000);
  bid_levels_.reserve(1000);
  ask_levels_.reserve(1000);

  // Reserve trade buffer for batching
  if (config_.batch_callbacks) {
    pending_trades_.reserve(config_.trade_batch_size);
  }
}

OrderBook::~OrderBook() = default;

OrderResult OrderBook::add_order(Order order) {
  OrderResult result{};
  result.order_id = order.id;

  // Fast path validations with branch prediction hints
  if (order.quantity == 0) [[unlikely]] {
    result.success = false;
    result.reject_reason = RejectReason::InvalidQuantity;
    return result;
  }

  if (order.type == OrderType::Limit && order.price <= 0) [[unlikely]] {
    result.success = false;
    result.reject_reason = RejectReason::InvalidPrice;
    return result;
  }

  if (order.type == OrderType::Market) [[unlikely]] {
    if (order.side == Side::Buy) {
      order.price = INVALID_PRICE;
    } else {
      order.price = 0;
    }
  }

  if (orders_.count(order.id)) [[unlikely]] {
    result.success = false;
    result.reject_reason = RejectReason::DuplicateOrderId;
    return result;
  }

  // post-only can't cross
  if (order.type == OrderType::PostOnly && config_.enable_post_only_rejection)
      [[unlikely]] {
    bool would_cross = false;
    if (order.side == Side::Buy && best_ask_ != INVALID_PRICE &&
        order.price >= best_ask_) {
      would_cross = true;
    } else if (order.side == Side::Sell && best_bid_ != INVALID_PRICE &&
               order.price <= best_bid_) {
      would_cross = true;
    }
    if (would_cross) {
      result.success = false;
      result.reject_reason = RejectReason::WouldCross;
      emit_execution_report(order, 0, 0);
      return result;
    }
  }

  if (order.tif == TimeInForce::FillOrKill ||
      order.type == OrderType::FillOrKill) [[unlikely]] {
    Quantity available = 0;
    auto &opposite_levels =
        (order.side == Side::Buy) ? ask_levels_ : bid_levels_;

    for (const auto &[price, level] : opposite_levels) {
      if (order.type == OrderType::Market || order.would_cross(price)) {
        available += level.total_quantity();
      }
      if (available >= order.quantity)
        break;
    }

    if (available < order.quantity) {
      result.success = false;
      result.reject_reason = RejectReason::InvalidQuantity;
      return result;
    }
  }

  // Main matching logic - hot path
  match_order(order);

  result.filled_qty = order.filled_qty;
  result.avg_fill_price =
      order.filled_qty > 0 ? (order.price * order.filled_qty) / order.filled_qty
                           : 0;

  // Handle time-in-force logic
  if (order.tif == TimeInForce::ImmediateOrCancel ||
      order.type == OrderType::ImmediateOrCancel) [[unlikely]] {
    if (!order.is_filled()) {
      emit_execution_report(order, order.filled_qty, result.avg_fill_price);
    }
  } else if (order.tif == TimeInForce::FillOrKill ||
             order.type == OrderType::FillOrKill) [[unlikely]] {
    if (!order.is_filled()) {
      result.success = false;
      result.reject_reason = RejectReason::InvalidQuantity;
      result.filled_qty = 0;
      return result;
    }
  } else if (!order.is_filled() && order.type == OrderType::Limit) [[likely]] {
    insert_resting_order(order);
  } else if (!order.is_filled() && order.type == OrderType::PostOnly)
      [[unlikely]] {
    insert_resting_order(order);
  }

  if (order.filled_qty > 0) [[likely]] {
    emit_execution_report(order, order.filled_qty, result.avg_fill_price);
  }

  // Flush any pending trades
  if (config_.batch_callbacks) [[likely]] {
    flush_trades();
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

  Order &order = it->second;
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

OrderResult OrderBook::replace_order(OrderId order_id, Price new_price,
                                     Quantity new_qty) {
  OrderResult result;
  result.success = false;

  auto it = orders_.find(order_id);
  if (it == orders_.end()) {
    return result;
  }

  Order &existing = it->second;

  // same price = amend in place (keep time priority)
  if (existing.price == new_price) {
    auto &levels = (existing.side == Side::Buy) ? bid_levels_ : ask_levels_;
    auto level_it = levels.find(existing.price);

    if (level_it != levels.end()) {
      level_it->second.modify_quantity(order_id, new_qty);
      existing.quantity = new_qty;

      result.success = true;
      maybe_emit_bbo();
      return result;
    }
  }

  // different price = cancel/replace, lose priority
  Side side = existing.side;
  Symbol symbol = existing.symbol;
  OrderType type = existing.type;
  TimeInForce tif = existing.tif;

  auto cancel_result = cancel_order(order_id);
  if (!cancel_result.success) {
    return cancel_result;
  }

  Order new_order(order_id, symbol, side, new_price, new_qty, type, tif);
  return add_order(new_order);
}

void OrderBook::match_order(Order &order) {
  auto &opposite_levels = (order.side == Side::Buy) ? ask_levels_ : bid_levels_;
  bool stopped_for_self_trade = false;

  while (!order.is_filled() && !stopped_for_self_trade) {
    Price best_opposite = (order.side == Side::Buy) ? best_ask_ : best_bid_;

    if (best_opposite == INVALID_PRICE || best_opposite == 0) {
      break;
    }

    bool can_match =
        (order.type == OrderType::Market) || order.would_cross(best_opposite);

    if (!can_match) {
      break;
    }

    auto level_it = opposite_levels.find(best_opposite);
    if (level_it == opposite_levels.end() || level_it->second.empty()) {
      if (order.side == Side::Buy) {
        update_best_ask();
      } else {
        update_best_bid();
      }
      continue;
    }

    PriceLevel &level = level_it->second;

    // Hot path: match against resting orders at this price
    while (!order.is_filled() && !level.empty()) [[likely]] {
      const OrderId resting_id = level.front_order_id();

      auto resting_it = orders_.find(resting_id);
      if (resting_it == orders_.end()) [[unlikely]] {
        level.pop_front();
        continue;
      }

      Order &resting = resting_it->second;

      // Self-trade prevention check
      if (config_.enable_self_trade_prevention) [[unlikely]] {
        if (std::abs(static_cast<int64_t>(order.id - resting.id)) < 100) {
          stopped_for_self_trade = true;
          break;
        }
      }

      // Calculate fill quantity
      const Quantity fill_qty =
          std::min(order.remaining(), resting.remaining());

      // Update orders
      order.fill(fill_qty);
      resting.fill(fill_qty);
      level.fill_front(fill_qty);

      // Emit trade (will be batched)
      if (order.side == Side::Buy) {
        emit_trade(order, resting, best_opposite, fill_qty, Side::Buy);
      } else {
        emit_trade(resting, order, best_opposite, fill_qty, Side::Sell);
      }

      // Remove filled resting order
      if (resting.is_filled()) [[likely]] {
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
  auto &levels = (order.side == Side::Buy) ? bid_levels_ : ask_levels_;

  // Get or create price level
  auto [it, inserted] = levels.try_emplace(order.price, order.price);
  it->second.add_order(order.id, order.remaining());

  // Store order
  orders_[order.id] = order;

  // Update best price
  if (order.side == Side::Buy) {
    if (best_bid_ == INVALID_PRICE || order.price > best_bid_) {
      best_bid_ = order.price;
    }
  } else {
    if (order.price < best_ask_) {
      best_ask_ = order.price;
    }
  }

  maybe_emit_bbo();
}

void OrderBook::remove_order_from_level(OrderId order_id, Side side,
                                        Price price) {
  auto &levels = (side == Side::Buy) ? bid_levels_ : ask_levels_;

  auto it = levels.find(price);
  if (it != levels.end()) {
    it->second.remove_order(order_id);
    if (it->second.empty()) {
      levels.erase(it);
    }
  }
}

void OrderBook::update_best_bid() {
  best_bid_ = INVALID_PRICE;
  for (const auto &[price, level] : bid_levels_) {
    if (!level.empty() && (best_bid_ == INVALID_PRICE || price > best_bid_)) {
      best_bid_ = price;
    }
  }
}

void OrderBook::update_best_ask() {
  best_ask_ = INVALID_PRICE;
  for (const auto &[price, level] : ask_levels_) {
    if (!level.empty() && price < best_ask_) {
      best_ask_ = price;
    }
  }
}

void OrderBook::emit_trade(const Order &buy, const Order &sell, Price price,
                           Quantity qty, Side aggressor) {
  if (!on_trade_)
    return;

  Trade trade;
  trade.seq_num = next_seq_num_++;
  trade.symbol = symbol_;
  trade.price = price;
  trade.quantity = qty;
  trade.buy_order_id = buy.id;
  trade.sell_order_id = sell.id;
  trade.timestamp = Timing::now_ns();
  trade.aggressor_side = aggressor;

  if (config_.batch_callbacks) [[likely]] {
    pending_trades_.push_back(trade);
    // Flush if batch is full
    if (pending_trades_.size() >= config_.trade_batch_size) [[unlikely]] {
      flush_trades();
    }
  } else {
    on_trade_(trade);
  }
}

void OrderBook::flush_trades() {
  if (pending_trades_.empty() || !on_trade_)
    return;

  for (const auto &trade : pending_trades_) {
    on_trade_(trade);
  }
  pending_trades_.clear();
}

void OrderBook::maybe_emit_bbo() {
  if (!on_bbo_)
    return;

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
  if (best_ask_ == INVALID_PRICE || best_bid_ == INVALID_PRICE) {
    return INVALID_PRICE;
  }
  return best_ask_ - best_bid_;
}

Price OrderBook::mid_price() const {
  if (best_ask_ == INVALID_PRICE || best_bid_ == INVALID_PRICE) {
    return 0;
  }
  return (best_bid_ + best_ask_) / 2;
}

size_t OrderBook::bid_level_count() const { return bid_levels_.size(); }
size_t OrderBook::ask_level_count() const { return ask_levels_.size(); }

std::vector<PriceLevelUpdate> OrderBook::get_bids(size_t depth) const {
  std::vector<PriceLevelUpdate> result;
  result.reserve(std::min(depth, bid_levels_.size()));

  // Collect all bid levels and sort by price descending
  std::vector<std::pair<Price, const PriceLevel *>> levels;
  for (const auto &[price, level] : bid_levels_) {
    if (!level.empty()) {
      levels.push_back({price, &level});
    }
  }

  std::sort(levels.begin(), levels.end(),
            [](const auto &a, const auto &b) { return a.first > b.first; });

  for (size_t i = 0; i < std::min(depth, levels.size()); ++i) {
    PriceLevelUpdate update;
    update.price = levels[i].first;
    update.quantity = levels[i].second->total_quantity();
    update.order_count = static_cast<uint32_t>(levels[i].second->order_count());
    result.push_back(update);
  }

  return result;
}

std::vector<PriceLevelUpdate> OrderBook::get_asks(size_t depth) const {
  std::vector<PriceLevelUpdate> result;
  result.reserve(std::min(depth, ask_levels_.size()));

  // Collect all ask levels and sort by price ascending
  std::vector<std::pair<Price, const PriceLevel *>> levels;
  for (const auto &[price, level] : ask_levels_) {
    if (!level.empty()) {
      levels.push_back({price, &level});
    }
  }

  std::sort(levels.begin(), levels.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });

  for (size_t i = 0; i < std::min(depth, levels.size()); ++i) {
    PriceLevelUpdate update;
    update.price = levels[i].first;
    update.quantity = levels[i].second->total_quantity();
    update.order_count = static_cast<uint32_t>(levels[i].second->order_count());
    result.push_back(update);
  }

  return result;
}

void OrderBook::emit_execution_report(const Order &order, Quantity filled,
                                      Price avg_price) {
  if (!on_execution_)
    return;

  ExecutionReport report;
  report.order_id = order.id;
  report.exec_id = next_seq_num_++; // Use sequence number as exec ID
  report.symbol = order.symbol;
  report.side = order.side;
  report.status = order.status;
  report.price = order.price;
  report.order_qty = order.quantity;
  report.filled_qty = filled;
  report.leaves_qty = order.quantity - filled;
  report.avg_price = avg_price;
  report.timestamp = Timing::now_ns();
  report.last_price = avg_price;
  report.last_qty = filled;

  on_execution_(report);
}

} // namespace exchange
