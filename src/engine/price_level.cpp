#include "engine/price_level.hpp"

#include <algorithm>

namespace exchange {

PriceLevel::PriceLevel(Price price)
    : price_(price)
    , total_quantity_(0)
    , order_count_(0)
    , head_(nullptr)
    , tail_(nullptr)
{}

PriceLevel::~PriceLevel() {
    // Clean up all nodes
    for (auto* node : nodes_) {
        delete node;
    }
}

PriceLevel::PriceLevel(PriceLevel&& other) noexcept
    : price_(other.price_)
    , total_quantity_(other.total_quantity_)
    , order_count_(other.order_count_)
    , head_(other.head_)
    , tail_(other.tail_)
    , nodes_(std::move(other.nodes_))
{
    other.head_ = nullptr;
    other.tail_ = nullptr;
    other.total_quantity_ = 0;
    other.order_count_ = 0;
}

PriceLevel& PriceLevel::operator=(PriceLevel&& other) noexcept {
    if (this != &other) {
        // Clean up existing
        for (auto* node : nodes_) {
            delete node;
        }
        
        price_ = other.price_;
        total_quantity_ = other.total_quantity_;
        order_count_ = other.order_count_;
        head_ = other.head_;
        tail_ = other.tail_;
        nodes_ = std::move(other.nodes_);
        
        other.head_ = nullptr;
        other.tail_ = nullptr;
        other.total_quantity_ = 0;
        other.order_count_ = 0;
    }
    return *this;
}

void PriceLevel::add_order(OrderId order_id, Quantity quantity) {
    auto* node = new OrderNode(order_id, quantity);
    nodes_.push_back(node);
    
    // Add to tail
    node->prev = tail_;
    node->next = nullptr;
    
    if (tail_) {
        tail_->next = node;
    } else {
        head_ = node;
    }
    tail_ = node;
    
    total_quantity_ += quantity;
    ++order_count_;
}

bool PriceLevel::remove_order(OrderId order_id) {
    OrderNode* node = find_node(order_id);
    if (!node) {
        return false;
    }
    
    total_quantity_ -= node->quantity;
    --order_count_;
    
    unlink(node);
    
    // Remove from nodes vector
    auto it = std::find(nodes_.begin(), nodes_.end(), node);
    if (it != nodes_.end()) {
        nodes_.erase(it);
    }
    
    delete node;
    return true;
}

bool PriceLevel::modify_quantity(OrderId order_id, Quantity new_quantity) {
    OrderNode* node = find_node(order_id);
    if (!node) {
        return false;
    }
    
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
    if (!head_) return;
    
    OrderNode* old_head = head_;
    total_quantity_ -= old_head->quantity;
    --order_count_;
    
    head_ = head_->next;
    if (head_) {
        head_->prev = nullptr;
    } else {
        tail_ = nullptr;
    }
    
    // Remove from nodes vector
    auto it = std::find(nodes_.begin(), nodes_.end(), old_head);
    if (it != nodes_.end()) {
        nodes_.erase(it);
    }
    
    delete old_head;
}

void PriceLevel::fill_front(Quantity qty) {
    if (!head_) return;
    
    if (qty >= head_->quantity) {
        pop_front();
    } else {
        total_quantity_ -= qty;
        head_->quantity -= qty;
    }
}

OrderNode* PriceLevel::find_node(OrderId order_id) {
    for (auto* node : nodes_) {
        if (node->order_id == order_id) {
            return node;
        }
    }
    return nullptr;
}

void PriceLevel::unlink(OrderNode* node) {
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        head_ = node->next;
    }
    
    if (node->next) {
        node->next->prev = node->prev;
    } else {
        tail_ = node->prev;
    }
}

}  // namespace exchange
