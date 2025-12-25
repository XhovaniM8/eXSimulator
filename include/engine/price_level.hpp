#pragma once

#include <deque>
#include <vector>

#include "core/types.hpp"

namespace exchange {

// Node in the order queue at a price level
// Uses intrusive list for O(1) removal
struct OrderNode {
    OrderId order_id;
    Quantity quantity;
    OrderNode* prev;
    OrderNode* next;
    
    OrderNode(OrderId id, Quantity qty) 
        : order_id(id), quantity(qty), prev(nullptr), next(nullptr) {}
};

// FIFO queue of orders at a single price level
// Optimized for:
// - O(1) insertion at tail
// - O(1) removal from head (for fills)
// - O(1) removal from middle (for cancels, using intrusive pointers)
class PriceLevel {
public:
    explicit PriceLevel(Price price = 0);
    ~PriceLevel();
    
    // Non-copyable due to intrusive list pointers
    PriceLevel(const PriceLevel&) = delete;
    PriceLevel& operator=(const PriceLevel&) = delete;
    
    // Movable
    PriceLevel(PriceLevel&& other) noexcept;
    PriceLevel& operator=(PriceLevel&& other) noexcept;
    
    // Add order to back of queue (FIFO)
    void add_order(OrderId order_id, Quantity quantity);
    
    // Remove specific order by ID
    // Returns true if found and removed
    bool remove_order(OrderId order_id);
    
    // Modify quantity of existing order
    // Returns true if found
    bool modify_quantity(OrderId order_id, Quantity new_quantity);
    
    // Get front order (best time priority)
    OrderId front_order_id() const;
    Quantity front_quantity() const;
    
    // Remove front order after fill
    void pop_front();
    
    // Fill front order partially
    void fill_front(Quantity qty);
    
    // Level aggregates
    Price price() const { return price_; }
    Quantity total_quantity() const { return total_quantity_; }
    size_t order_count() const { return order_count_; }
    bool empty() const { return order_count_ == 0; }
    
private:
    Price price_;
    Quantity total_quantity_;
    size_t order_count_;
    
    // Intrusive doubly-linked list for O(1) operations
    OrderNode* head_;
    OrderNode* tail_;
    
    // Map from order_id to node for O(1) cancel lookup
    // Using vector + linear search for small levels
    // Could use unordered_map for levels with many orders
    std::vector<OrderNode*> nodes_;
    
    // Find node by order ID
    OrderNode* find_node(OrderId order_id);
    
    // Unlink node from list
    void unlink(OrderNode* node);
};

// Alternative: Array-based price level for ultra-low latency
// Pre-allocates fixed number of order slots
// Trades memory for guaranteed no-allocation on hot path
template <size_t MaxOrders = 64>
class FixedPriceLevel {
public:
    explicit FixedPriceLevel(Price price = 0) 
        : price_(price), total_quantity_(0), head_(0), tail_(0), count_(0) {}
    
    bool add_order(OrderId order_id, Quantity quantity) {
        if (count_ >= MaxOrders) return false;
        
        size_t idx = tail_;
        orders_[idx] = {order_id, quantity, true};
        tail_ = (tail_ + 1) % MaxOrders;
        total_quantity_ += quantity;
        ++count_;
        return true;
    }
    
    // ... similar interface to PriceLevel
    
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

}  // namespace exchange
