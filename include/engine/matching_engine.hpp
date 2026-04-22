#pragma once

#include "core/order.hpp"
#include "core/trade.hpp"
#include "engine/order_book.hpp"
#include "utils/spsc_queue.hpp"

namespace exchange {

// Inbound message types
enum class MessageType : uint8_t {
  NewOrder = 0,
  CancelOrder = 1,
  ReplaceOrder = 2
};

// Generic message wrapper for queue
struct InboundMessage {
  InboundMessage() : type(MessageType::NewOrder) {}

  MessageType type;
  Order order;      // For new orders
  OrderId order_id; // For cancel/replace
  Symbol symbol;    // For cancel/replace
  Price new_price;  // For replace
  Quantity new_qty; // For replace
};

// Engine configuration
struct EngineConfig {
  size_t num_symbols = 100;
  size_t inbound_queue_size = 65536;
  size_t outbound_queue_size = 65536;
  OrderBookConfig book_config;
};

// Matching engine statistics
struct EngineStats {
  uint64_t orders_received;
  uint64_t orders_matched;
  uint64_t orders_cancelled;
  uint64_t trades_executed;
  uint64_t total_volume;
  uint64_t messages_processed;
};

// The main matching engine
// Owns order books for multiple symbols
// Processes messages from inbound queue
class MatchingEngine {
public:
  explicit MatchingEngine(const EngineConfig &config = {});
  ~MatchingEngine();

  // Non-copyable, non-movable
  MatchingEngine(const MatchingEngine &) = delete;
  MatchingEngine &operator=(const MatchingEngine &) = delete;

  // Add a symbol to trade
  void add_symbol(const Symbol &symbol);
  void add_symbol(const char *symbol);

  // Direct order submission (for single-threaded use)
  OrderResult submit_order(Order order);
  OrderResult cancel_order(const Symbol &symbol, OrderId order_id);
  OrderResult replace_order(const Symbol &symbol, OrderId order_id,
                            Price new_price, Quantity new_qty);

  // Queue-based submission (for multi-threaded use)
  bool enqueue_order(Order order);
  bool enqueue_cancel(const Symbol &symbol, OrderId order_id);
  bool enqueue_replace(const Symbol &symbol, OrderId order_id, Price new_price,
                       Quantity new_qty);

  // Process all pending messages from queue
  // Returns number of messages processed
  size_t process_queue();

  // Process single message from queue
  // Returns false if queue was empty
  bool process_one();

  // Run engine loop (blocking)
  void run();
  void stop();

  // Get order book for symbol
  OrderBook *get_book(const Symbol &symbol);
  const OrderBook *get_book(const Symbol &symbol) const;

  // Statistics
  const EngineStats &stats() const { return stats_; }
  void reset_stats();

  // Callbacks (forwarded to all order books)
  void set_trade_callback(TradeCallback cb);
  void set_execution_callback(ExecutionCallback cb);

private:
  EngineConfig config_;

  // Symbol hash function
  struct SymbolHash {
    size_t operator()(const Symbol &s) const {
      size_t hash = 14695981039346656037ULL;
      for (size_t i = 0; i < SYMBOL_SIZE; ++i) {
        hash ^= static_cast<size_t>(s.data[i]);
        hash *= 1099511628211ULL;
      }
      return hash;
    }
  };

  // Order books by symbol
  std::unordered_map<Symbol, std::unique_ptr<OrderBook>, SymbolHash> books_;
  std::unordered_map<Symbol, OrderBook *, SymbolHash> symbol_to_book_;

  // Inbound message queue
  using InboundQueue = SPSCQueue<InboundMessage, 65536>;
  std::unique_ptr<InboundQueue> inbound_queue_;

  // Statistics
  EngineStats stats_;

  // Running flag
  std::atomic<bool> running_;

  // Shared callbacks
  TradeCallback trade_callback_;
  ExecutionCallback execution_callback_;
};

// Helper to run engine in dedicated thread
// Note: Currently a synchronous wrapper. Full threading support would require:
//   - A worker thread running engine.process_one() in a loop
//   - Proper synchronization of start/stop with the worker
//   - Thread-safe shutdown signaling
// For now, users can call engine.process_one() directly from their own threads.
class EngineRunner {
public:
  explicit EngineRunner(MatchingEngine &engine);
  ~EngineRunner();

  void start();
  void stop();
  bool is_running() const;

private:
  MatchingEngine &engine_;
  std::atomic<bool> running_;
};

} // namespace exchange
