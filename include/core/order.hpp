#pragma once
#include "core/types.hpp"
#include <algorithm>
#include <cstddef>

namespace exchange {
struct alignas(64) Order {
  // Hot fields (frequently accessed during matching) - first 32 bytes
  OrderId id = 0;                        // 8 bytes
  Price price = 0;                       // 8 bytes
  Quantity quantity = 0;                 // 4 bytes
  Quantity filled_qty = 0;               // 4 bytes
  Side side = Side::Buy;                 // 1 byte
  OrderType type = OrderType::Limit;     // 1 byte
  TimeInForce tif = TimeInForce::Day;    // 1 byte
  OrderStatus status = OrderStatus::New; // 1 byte
  uint32_t _pad1 = 0;                    // 4 bytes padding

  // Cold fields (accessed less frequently)
  Symbol symbol{};         // 8 bytes
  Timestamp timestamp = 0; // 8 bytes

  // Constructor
  Order() = default;

  Order(OrderId id_, Symbol symbol_, Side side_, Price price_, Quantity qty_,
        OrderType type_ = OrderType::Limit, TimeInForce tif_ = TimeInForce::Day)
      : id(id_), price(price_), quantity(qty_), filled_qty(0), side(side_),
        type(type_), tif(tif_), status(OrderStatus::New), _pad1(0),
        symbol(symbol_), timestamp(0) {}

  // Remaining quantity
  Quantity remaining() const { return quantity - filled_qty; }

  // Is order fully filled?
  bool is_filled() const { return filled_qty >= quantity; }

  // Can this order be matched?
  bool is_active() const {
    return status == OrderStatus::New || status == OrderStatus::PartiallyFilled;
  }

  // Fill order by given quantity, returns actual fill amount
  Quantity fill(Quantity fill_qty) {
    Quantity actual = std::min(fill_qty, remaining());
    filled_qty += actual;
    if (is_filled()) {
      status = OrderStatus::Filled;
    } else if (filled_qty > 0) {
      status = OrderStatus::PartiallyFilled;
    }
    return actual;
  }

  // Cancel order
  void cancel() { status = OrderStatus::Cancelled; }

  // Price comparison for matching
  // Buy orders: higher price has priority
  // Sell orders: lower price has priority
  bool has_price_priority_over(const Order &other) const {
    if (side == Side::Buy) {
      return price > other.price;
    } else {
      return price < other.price;
    }
  }

  // Would this order cross with given price?
  bool would_cross(Price other_price) const {
    if (side == Side::Buy) {
      return price >= other_price;
    } else {
      return price <= other_price;
    }
  }
};
// Compile-time layout verification
static_assert(sizeof(Order) == 64, "Order should be 64 bytes (one cache line)");
static_assert(alignof(Order) == 64, "Order should be cache-line aligned");
static_assert(offsetof(Order, price) == 8, "Price should be at offset 8");
} // namespace exchange
