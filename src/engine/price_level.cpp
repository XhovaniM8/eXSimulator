#include "engine/price_level.hpp"

namespace exchange {

PriceLevel::PriceLevel(Price price)
    : price_(price), total_quantity_(0), order_count_(0), head_(nullptr),
      tail_(nullptr) {}

PriceLevel::~PriceLevel() {
  OrderNode *cur = head_;
  while (cur) {
    OrderNode *next = cur->next;
    delete cur;
    cur = next;
  }
}

PriceLevel::PriceLevel(PriceLevel &&other) noexcept
    : price_(other.price_), total_quantity_(other.total_quantity_),
      order_count_(other.order_count_), head_(other.head_), tail_(other.tail_),
      node_index_(std::move(other.node_index_)) {
  other.head_ = nullptr;
  other.tail_ = nullptr;
  other.total_quantity_ = 0;
  other.order_count_ = 0;
}

PriceLevel &PriceLevel::operator=(PriceLevel &&other) noexcept {
  if (this != &other) {
    // Clean up existing nodes via linked list
    OrderNode *cur = head_;
    while (cur) {
      OrderNode *next = cur->next;
      delete cur;
      cur = next;
    }

    price_ = other.price_;
    total_quantity_ = other.total_quantity_;
    order_count_ = other.order_count_;
    head_ = other.head_;
    tail_ = other.tail_;
    node_index_ = std::move(other.node_index_);

    other.head_ = nullptr;
    other.tail_ = nullptr;
    other.total_quantity_ = 0;
    other.order_count_ = 0;
  }
  return *this;
}

void PriceLevel::add_order(OrderId order_id, Quantity quantity) {
  auto *node = new OrderNode(order_id, quantity);
  node_index_[order_id] = node;

  node->prev = tail_;
  node->next = nullptr;
  if (tail_)
    tail_->next = node;
  else
    head_ = node;
  tail_ = node;

  total_quantity_ += quantity;
  ++order_count_; // was ++order_count (missing underscore)
}

bool PriceLevel::remove_order(OrderId order_id) {
  OrderNode *node = find_node(order_id); // O(1) now
  if (!node)
    return false;

  total_quantity_ -= node->quantity;
  --order_count_;
  unlink(node);
  node_index_.erase(order_id);
  delete node;
  return true;
}

bool PriceLevel::modify_quantity(OrderId order_id, Quantity new_quantity) {
  OrderNode *node = find_node(order_id);
  if (!node)
    return false;

  total_quantity_ -= node->quantity;
  total_quantity_ += new_quantity;
  node->quantity = new_quantity;
  return true;
}

OrderId PriceLevel::front_order_id() const {
  return head_ ? head_->order_id : INVALID_ORDER_ID;
}

Quantity PriceLevel::front_quantity() const {
  return head_ ? head_->quantity : 0;
}

void PriceLevel::pop_front() {
  if (!head_)
    return;

  OrderNode *old_head = head_;
  total_quantity_ -= old_head->quantity;
  --order_count_;

  head_ = head_->next;
  if (head_)
    head_->prev = nullptr;
  else
    tail_ = nullptr;

  node_index_.erase(old_head->order_id); // O(1), no more std::find scan
  delete old_head;
}

void PriceLevel::fill_front(Quantity qty) {
  if (!head_)
    return;
  if (qty >= head_->quantity) {
    pop_front();
  } else {
    total_quantity_ -= qty;
    head_->quantity -= qty;
  }
}

OrderNode *PriceLevel::find_node(OrderId order_id) {
  auto it = node_index_.find(order_id); // O(1), no more linear scan
  return it != node_index_.end() ? it->second : nullptr;
}

void PriceLevel::unlink(OrderNode *node) {
  if (node->prev)
    node->prev->next = node->next;
  else
    head_ = node->next;

  if (node->next)
    node->next->prev = node->prev;
  else
    tail_ = node->prev;
}

} // namespace exchange
