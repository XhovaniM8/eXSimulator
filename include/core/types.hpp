#pragma once

#include <cstdint>
#include <cstring>
#include <limits>

namespace exchange {

// Fixed-point price representation (avoid floating point on hot path)
// Price is stored as integer * PRICE_SCALE
// e.g., $123.45 stored as 12345 with PRICE_SCALE = 100
constexpr int64_t PRICE_SCALE = 10000;  // 4 decimal places

using Price = int64_t;
using Quantity = uint32_t;
using OrderId = uint64_t;
using SequenceNum = uint64_t;
using Timestamp = uint64_t;  // nanoseconds since epoch

// Symbol represented as fixed-size array for cache efficiency
// No heap allocation, fits in cache line with other order data
constexpr size_t SYMBOL_SIZE = 8;
struct Symbol {
    char data[SYMBOL_SIZE]{};
    
    Symbol() = default;
    explicit Symbol(const char* s) {
        std::strncpy(data, s, SYMBOL_SIZE - 1);
    }
    
    bool operator==(const Symbol& other) const {
        return std::memcmp(data, other.data, SYMBOL_SIZE) == 0;
    }
    
    bool operator<(const Symbol& other) const {
        return std::memcmp(data, other.data, SYMBOL_SIZE) < 0;
    }
};

// Side enum with explicit underlying type for binary serialization
enum class Side : uint8_t {
    Buy = 0,
    Sell = 1
};

// Order type
enum class OrderType : uint8_t {
    Limit = 0,
    Market = 1,
    ImmediateOrCancel = 2,
    FillOrKill = 3,
    PostOnly = 4  // Maker only, reject if would cross
};

// Time in force
enum class TimeInForce : uint8_t {
    Day = 0,
    GoodTillCancel = 1,
    ImmediateOrCancel = 2,
    FillOrKill = 3
};

// Order status for reporting
enum class OrderStatus : uint8_t {
    New = 0,
    PartiallyFilled = 1,
    Filled = 2,
    Cancelled = 3,
    Rejected = 4
};

// Sentinel values
constexpr Price INVALID_PRICE = std::numeric_limits<Price>::max();
constexpr OrderId INVALID_ORDER_ID = 0;
constexpr Quantity MAX_QUANTITY = std::numeric_limits<Quantity>::max();

// Helper functions
inline Price price_from_double(double p) {
    return static_cast<Price>(p * PRICE_SCALE);
}

inline double price_to_double(Price p) {
    return static_cast<double>(p) / PRICE_SCALE;
}

// Compile-time checks
static_assert(sizeof(Symbol) == SYMBOL_SIZE, "Symbol size mismatch");
static_assert(sizeof(Side) == 1, "Side should be 1 byte");
static_assert(sizeof(OrderType) == 1, "OrderType should be 1 byte");

}  // namespace exchange
