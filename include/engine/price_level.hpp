#pragma once

#include "core/types.hpp"
#include <array>
#include <unordered_map>

namespace exchange {
struct OrderNode {
  OrderId order_id;
  Quantity quantity;
  OrderNode *prev;
  OrderNode *next;

  OrderNode(OrderId id, Quantity qty)
      : order_id(id), quantity(qty), prev(nullptr), next(nullptr) {}
};

class PriceLevel {
public:
  explicit PriceLevel(Price price = 0);
  ~PriceLevel();

  PriceLevel(const PriceLevel &) = delete;
  PriceLevel &operator=(const PriceLevel &) = delete;

  PriceLevel(PriceLevel &&other) noexcept;
  PriceLevel &operator=(PriceLevel &&other) noexcept;

  void add_order(OrderId order_id, Quantity quantity);

  bool remove_order(OrderId order_id);

  bool modify_quantity(OrderId order_id, Quantity new_quantity);

  OrderId front_order_id() const;
  Quantity front_quantity() const;

  void pop_front();

  void fill_front(Quantity qty);

  Price price() const { return price_; }
  Quantity total_quantity() const { return total_quantity_; }
  size_t order_count() const { return order_count_; }
  bool empty() const { return order_count_ == 0; }

private:
  Price price_;
  Quantity total_quantity_;
  size_t order_count_;

  OrderNode *head_;
  OrderNode *tail_;

  std::unordered_map<OrderId, OrderNode *> node_index_;
  OrderNode *find_node(OrderId order_id);
  void unlink(OrderNode *node);
};

template <size_t MaxOrders = 64> class FixedPriceLevel {
public:
  explicit FixedPriceLevel(Price price = 0)
      : price_(price), total_quantity_(0), head_(0), tail_(0), count_(0) {}

  bool add_order(OrderId order_id, Quantity quantity) {
    if (count_ >= MaxOrders)
      return false;

    size_t idx = tail_;
    orders_[idx] = {order_id, quantity, true};
    tail_ = (tail_ + 1) % MaxOrders;
    total_quantity_ += quantity;
    ++count_;
    return true;
  }

  Price price() const { return price_; }
  Quantity total_quantity() const { return total_quantity_; }
  size_t order_count() const { return count_; }
  bool empty() const { return count_ == 0; }
  bool full() const { return count_ >= MaxOrders; }

private:
  struct Slot {
    OrderId order_id;
    Quantity quantity;
    bool active;
  };

  Price price_;
  Quantity total_quantity_;
  size_t head_;
  size_t tail_;
  size_t count_;
  std::array<Slot, MaxOrders> orders_;
};

} // namespace exchange
