#pragma once

#include "core/types.hpp"

namespace exchange {

// Trade execution report
// Represents a single fill between two orders
struct Trade {
    SequenceNum seq_num;       // Exchange sequence number
    Symbol symbol;
    Price price;
    Quantity quantity;
    OrderId buy_order_id;
    OrderId sell_order_id;
    Timestamp timestamp;
    
    // Aggressor side (who initiated the trade by crossing the spread)
    Side aggressor_side;
};

// Market data snapshot for a single price level
struct PriceLevelUpdate {
    Price price;
    Quantity quantity;     // Total quantity at this level
    uint32_t order_count;  // Number of orders at this level
};

// Market data message types
enum class MarketDataType : uint8_t {
    Trade = 0,
    BestBidOffer = 1,
    DepthUpdate = 2,
    Snapshot = 3
};

// BBO (Best Bid/Offer) update
struct BBOUpdate {
    Symbol symbol;
    Price bid_price;
    Quantity bid_qty;
    Price ask_price;
    Quantity ask_qty;
    Timestamp timestamp;
};

// Execution report sent back to order submitter
struct ExecutionReport {
    OrderId order_id;
    OrderId exec_id;       // Unique execution ID
    Symbol symbol;
    Side side;
    OrderStatus status;
    Price price;
    Quantity order_qty;
    Quantity filled_qty;
    Quantity leaves_qty;   // remaining quantity
    Price avg_price;       // average fill price
    Timestamp timestamp;
    
    // Optional: last fill info
    Price last_price;
    Quantity last_qty;
};

// Cancel/replace request
struct CancelRequest {
    OrderId order_id;
    Symbol symbol;
};

struct ReplaceRequest {
    OrderId order_id;
    Symbol symbol;
    Price new_price;       // INVALID_PRICE = no change
    Quantity new_qty;      // 0 = no change
};

// Reject reasons
enum class RejectReason : uint8_t {
    None = 0,
    UnknownSymbol = 1,
    InvalidPrice = 2,
    InvalidQuantity = 3,
    OrderNotFound = 4,
    WouldCross = 5,        // For post-only orders
    DuplicateOrderId = 6,
    ExchangeClosed = 7,
    RateLimitExceeded = 8,
    InsufficientFunds = 9
};

}  // namespace exchange
