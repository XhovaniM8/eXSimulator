`timescale 1ns / 1ps
//------------------------------------------------------------------------------
// Package: exsim_types_pkg
// Shared type definitions between FPGA and C++ simulator
// Keep in sync with include/core/types.hpp
//------------------------------------------------------------------------------

package exsim_types_pkg;

    // Price representation: fixed-point with 4 decimal places
    // e.g., 15025 = $150.25
    // Matches C++ Price type
    typedef logic signed [63:0] price_t;
    
    // Quantity: unsigned 32-bit
    typedef logic [31:0] quantity_t;
    
    // Order ID: 64-bit unique identifier
    typedef logic [63:0] order_id_t;
    
    // Symbol: 8-byte padded (e.g., "AAPL    ")
    typedef logic [63:0] symbol_t;
    
    // Timestamp: nanoseconds since epoch
    typedef logic [63:0] timestamp_t;
    
    // Side enum
    typedef enum logic [0:0] {
        SIDE_BUY  = 1'b0,
        SIDE_SELL = 1'b1
    } side_t;
    
    // Order type enum
    typedef enum logic [1:0] {
        ORDER_LIMIT  = 2'b00,
        ORDER_MARKET = 2'b01,
        ORDER_CANCEL = 2'b10
    } order_type_t;
    
    // Order status enum
    typedef enum logic [2:0] {
        STATUS_NEW      = 3'b000,
        STATUS_PARTIAL  = 3'b001,
        STATUS_FILLED   = 3'b010,
        STATUS_CANCELED = 3'b011,
        STATUS_REJECTED = 3'b100
    } order_status_t;
    
    // Order struct - matches C++ Order layout
    // Total: 32 bytes (256 bits) for AXI efficiency
    typedef struct packed {
        order_id_t   id;         // 64 bits
        price_t      price;      // 64 bits
        quantity_t   quantity;   // 32 bits
        quantity_t   filled_qty; // 32 bits
        symbol_t     symbol;     // 64 bits - simplified, could use index
    } order_t; // Note: side, type stored separately or in unused bits
    
    // Simplified order for matching (hot path)
    // 128 bits = fits nicely in AXI-Stream
    typedef struct packed {
        order_id_t   id;        // 64 bits
        price_t      price;     // 64 bits  
        quantity_t   quantity;  // 32 bits
        side_t       side;      // 1 bit
        order_type_t otype;     // 2 bits
        logic [28:0] reserved;  // Padding to 128 bits
    } order_slim_t;
    
    // Trade output
    typedef struct packed {
        order_id_t   buy_order_id;   // 64 bits
        order_id_t   sell_order_id;  // 64 bits
        price_t      price;          // 64 bits
        quantity_t   quantity;       // 32 bits
        timestamp_t  timestamp;      // 64 bits
    } trade_t;
    
    // BBO (Best Bid/Offer) output
    typedef struct packed {
        price_t    bid_price;    // 64 bits
        quantity_t bid_qty;      // 32 bits
        price_t    ask_price;    // 64 bits
        quantity_t ask_qty;      // 32 bits
    } bbo_t;

endpackage
