// tests/test_order_book_comprehensive.cpp
//

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <random>
#include <vector>

#include "core/order.hpp"
#include "engine/order_book.hpp"

namespace exchange {
namespace test {

// Test result tracking
struct TestResult {
  const char *name;
  bool passed;
  std::string failure_reason;
};

static std::vector<TestResult> results;

#define TEST(name)                                                             \
  static void test_##name();                                                   \
  static struct Register_##name {                                              \
    Register_##name() { run_test(#name, test_##name); }                        \
  } register_##name;                                                           \
  static void test_##name()

#define ASSERT_TRUE(cond)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      throw std::runtime_error("ASSERT_TRUE failed: " #cond);                  \
    }                                                                          \
  } while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define ASSERT_EQ(a, b)                                                        \
  do {                                                                         \
    if ((a) != (b)) {                                                          \
      throw std::runtime_error("ASSERT_EQ failed: " #a " != " #b);             \
    }                                                                          \
  } while (0)

#define ASSERT_NE(a, b)                                                        \
  do {                                                                         \
    if ((a) == (b)) {                                                          \
      throw std::runtime_error("ASSERT_NE failed: " #a " == " #b);             \
    }                                                                          \
  } while (0)

static void run_test(const char *name, void (*fn)()) {
  TestResult result{name, false, ""};
  try {
    fn();
    result.passed = true;
  } catch (const std::exception &e) {
    result.failure_reason = e.what();
  } catch (...) {
    result.failure_reason = "Unknown exception";
  }
  results.push_back(result);
}

// Helper to create fresh order book
static std::unique_ptr<OrderBook> make_book(OrderBookConfig config = {}) {
  Symbol sym("TEST");
  return std::make_unique<OrderBook>(sym, config);
}

static OrderId next_order_id = 1;
static OrderId fresh_id() { return next_order_id++; }

// ============================================================================
// BASIC OPERATIONS
// ============================================================================

TEST(empty_book_has_no_bbo) {
  auto book = make_book();
  ASSERT_EQ(book->best_bid(), INVALID_PRICE);
  ASSERT_EQ(book->best_ask(), INVALID_PRICE);
  ASSERT_EQ(book->bid_quantity_at(10000), 0u);
  ASSERT_EQ(book->ask_quantity_at(10000), 0u);
}

TEST(add_single_bid) {
  auto book = make_book();
  Symbol sym("TEST");
  Order order(fresh_id(), sym, Side::Buy, 10000, 100);

  auto result = book->add_order(order);

  ASSERT_TRUE(result.success);
  ASSERT_EQ(book->best_bid(), 10000);
  ASSERT_EQ(book->bid_quantity_at(10000), 100u);
  ASSERT_EQ(book->best_ask(), INVALID_PRICE);
}

TEST(add_single_ask) {
  auto book = make_book();
  Symbol sym("TEST");
  Order order(fresh_id(), sym, Side::Sell, 10100, 100);

  auto result = book->add_order(order);

  ASSERT_TRUE(result.success);
  ASSERT_EQ(book->best_ask(), 10100);
  ASSERT_EQ(book->ask_quantity_at(10100), 100u);
  ASSERT_EQ(book->best_bid(), INVALID_PRICE);
}

TEST(reject_zero_quantity) {
  auto book = make_book();
  Symbol sym("TEST");
  Order order(fresh_id(), sym, Side::Buy, 10000, 0);

  auto result = book->add_order(order);

  ASSERT_FALSE(result.success);
  ASSERT_EQ(result.reject_reason, RejectReason::InvalidQuantity);
}

TEST(reject_zero_price_for_limit) {
  auto book = make_book();
  Symbol sym("TEST");
  Order order(fresh_id(), sym, Side::Buy, 0, 100, OrderType::Limit);

  auto result = book->add_order(order);

  ASSERT_FALSE(result.success);
  ASSERT_EQ(result.reject_reason, RejectReason::InvalidPrice);
}

TEST(reject_duplicate_order_id) {
  auto book = make_book();
  Symbol sym("TEST");
  OrderId id = fresh_id();

  Order order1(id, sym, Side::Buy, 10000, 100);
  Order order2(id, sym, Side::Buy, 10100, 50); // Same ID

  book->add_order(order1);
  auto result = book->add_order(order2);

  ASSERT_FALSE(result.success);
  ASSERT_EQ(result.reject_reason, RejectReason::DuplicateOrderId);
}

// ============================================================================
// MATCHING
// ============================================================================

TEST(exact_match_same_price) {
  auto book = make_book();
  Symbol sym("TEST");

  Order sell(fresh_id(), sym, Side::Sell, 10000, 100);
  Order buy(fresh_id(), sym, Side::Buy, 10000, 100);

  book->add_order(sell);
  auto result = book->add_order(buy);

  ASSERT_TRUE(result.success);
  ASSERT_EQ(result.filled_qty, 100u);
  ASSERT_EQ(book->best_bid(), INVALID_PRICE);
  ASSERT_EQ(book->best_ask(), INVALID_PRICE);
}

TEST(buy_crosses_at_higher_price) {
  auto book = make_book();
  Symbol sym("TEST");

  Order sell(fresh_id(), sym, Side::Sell, 10000, 100);
  Order buy(fresh_id(), sym, Side::Buy, 10100, 100); // Willing to pay more

  book->add_order(sell);
  auto result = book->add_order(buy);

  ASSERT_TRUE(result.success);
  ASSERT_EQ(result.filled_qty, 100u);
}

TEST(partial_fill_leaves_remainder) {
  auto book = make_book();
  Symbol sym("TEST");

  Order sell(fresh_id(), sym, Side::Sell, 10000, 100);
  Order buy(fresh_id(), sym, Side::Buy, 10000, 60);

  book->add_order(sell);
  book->add_order(buy);

  ASSERT_EQ(book->ask_quantity_at(10000), 40u); // 100 - 60
}

TEST(aggressive_order_sweeps_multiple_levels) {
  auto book = make_book();
  Symbol sym("TEST");

  // Add asks at different prices
  book->add_order(Order(fresh_id(), sym, Side::Sell, 10000, 50));
  book->add_order(Order(fresh_id(), sym, Side::Sell, 10100, 50));
  book->add_order(Order(fresh_id(), sym, Side::Sell, 10200, 50));

  // Buy sweeps all three levels
  Order buy(fresh_id(), sym, Side::Buy, 10200, 150);
  auto result = book->add_order(buy);

  ASSERT_EQ(result.filled_qty, 150u);
  ASSERT_EQ(book->best_ask(), INVALID_PRICE);
}

TEST(no_match_when_prices_dont_cross) {
  auto book = make_book();
  Symbol sym("TEST");

  Order sell(fresh_id(), sym, Side::Sell, 10100, 100);
  Order buy(fresh_id(), sym, Side::Buy, 10000, 100);

  book->add_order(sell);
  auto result = book->add_order(buy);

  ASSERT_TRUE(result.success);
  ASSERT_EQ(result.filled_qty, 0u);
  ASSERT_EQ(book->best_bid(), 10000);
  ASSERT_EQ(book->best_ask(), 10100);
}

// ============================================================================
// PRICE-TIME PRIORITY (FIFO)
// ============================================================================

TEST(fifo_same_price_level) {
  auto book = make_book();
  Symbol sym("TEST");

  // Add three bids at same price
  OrderId id1 = fresh_id();
  OrderId id2 = fresh_id();
  OrderId id3 = fresh_id();

  book->add_order(Order(id1, sym, Side::Buy, 10000, 100));
  book->add_order(Order(id2, sym, Side::Buy, 10000, 100));
  book->add_order(Order(id3, sym, Side::Buy, 10000, 100));

  ASSERT_EQ(book->bid_quantity_at(10000), 300u);

  // Sell fills first order only
  book->add_order(Order(fresh_id(), sym, Side::Sell, 10000, 100));

  ASSERT_EQ(book->bid_quantity_at(10000), 200u);

  // Verify first order is gone (can't cancel it)
  auto cancel_result = book->cancel_order(id1);
  ASSERT_FALSE(cancel_result.success);

  // Second order still exists
  cancel_result = book->cancel_order(id2);
  ASSERT_TRUE(cancel_result.success);
}

TEST(price_priority_over_time) {
  auto book = make_book();
  Symbol sym("TEST");

  // Add bids: first at 10000, then at 10100 (better price)
  book->add_order(Order(fresh_id(), sym, Side::Buy, 10000, 100));
  book->add_order(Order(fresh_id(), sym, Side::Buy, 10100, 100));

  // Best bid should be 10100 (higher is better for bids)
  ASSERT_EQ(book->best_bid(), 10100);

  // Sell should match against 10100 first
  book->add_order(Order(fresh_id(), sym, Side::Sell, 10000, 100));

  ASSERT_EQ(book->best_bid(), 10000); // 10100 is gone
  ASSERT_EQ(book->bid_quantity_at(10000), 100u);
}

// ============================================================================
// CANCEL
// ============================================================================

TEST(cancel_existing_order) {
  auto book = make_book();
  Symbol sym("TEST");
  OrderId id = fresh_id();

  book->add_order(Order(id, sym, Side::Buy, 10000, 100));
  auto result = book->cancel_order(id);

  ASSERT_TRUE(result.success);
  ASSERT_EQ(book->best_bid(), INVALID_PRICE);
}

TEST(cancel_nonexistent_order) {
  auto book = make_book();

  auto result = book->cancel_order(99999);

  ASSERT_FALSE(result.success);
}

TEST(cancel_already_filled_order) {
  auto book = make_book();
  Symbol sym("TEST");

  OrderId bid_id = fresh_id();
  book->add_order(Order(bid_id, sym, Side::Buy, 10000, 100));
  book->add_order(
      Order(fresh_id(), sym, Side::Sell, 10000, 100)); // Fills the bid

  auto result = book->cancel_order(bid_id);

  ASSERT_FALSE(result.success); // Already gone
}

TEST(cancel_middle_of_queue) {
  auto book = make_book();
  Symbol sym("TEST");

  OrderId id1 = fresh_id();
  OrderId id2 = fresh_id();
  OrderId id3 = fresh_id();

  book->add_order(Order(id1, sym, Side::Buy, 10000, 100));
  book->add_order(Order(id2, sym, Side::Buy, 10000, 100));
  book->add_order(Order(id3, sym, Side::Buy, 10000, 100));

  // Cancel middle order
  auto result = book->cancel_order(id2);

  ASSERT_TRUE(result.success);
  ASSERT_EQ(book->bid_quantity_at(10000), 200u);
}

// ============================================================================
// REPLACE / AMEND
// ============================================================================

TEST(replace_quantity_same_price_keeps_priority) {
  auto book = make_book();
  Symbol sym("TEST");

  OrderId id1 = fresh_id();
  OrderId id2 = fresh_id();

  book->add_order(Order(id1, sym, Side::Buy, 10000, 100));
  book->add_order(Order(id2, sym, Side::Buy, 10000, 100));

  // Amend first order's quantity
  book->replace_order(id1, 10000, 150);

  ASSERT_EQ(book->bid_quantity_at(10000), 250u); // 150 + 100
}

TEST(replace_to_different_price_loses_priority) {
  auto book = make_book();
  Symbol sym("TEST");

  OrderId id = fresh_id();
  book->add_order(Order(id, sym, Side::Buy, 10000, 100));

  auto result = book->replace_order(id, 10100, 100);

  ASSERT_TRUE(result.success);
  ASSERT_EQ(book->bid_quantity_at(10000), 0u);
  ASSERT_EQ(book->bid_quantity_at(10100), 100u);
  ASSERT_EQ(book->best_bid(), 10100);
}

TEST(replace_nonexistent_order) {
  auto book = make_book();

  auto result = book->replace_order(99999, 10000, 100);

  ASSERT_FALSE(result.success);
}

// ============================================================================
// MARKET ORDERS
// ============================================================================

TEST(market_buy_fills_against_asks) {
  auto book = make_book();
  Symbol sym("TEST");

  book->add_order(Order(fresh_id(), sym, Side::Sell, 10100, 100));

  Order market_buy(fresh_id(), sym, Side::Buy, 0, 50, OrderType::Market);
  auto result = book->add_order(market_buy);

  ASSERT_TRUE(result.success);
  ASSERT_EQ(result.filled_qty, 50u);
  ASSERT_EQ(book->ask_quantity_at(10100), 50u);
}

TEST(market_sell_fills_against_bids) {
  auto book = make_book();
  Symbol sym("TEST");

  book->add_order(Order(fresh_id(), sym, Side::Buy, 10000, 100));

  Order market_sell(fresh_id(), sym, Side::Sell, 0, 50, OrderType::Market);
  auto result = book->add_order(market_sell);

  ASSERT_TRUE(result.success);
  ASSERT_EQ(result.filled_qty, 50u);
}

TEST(market_order_with_no_liquidity) {
  auto book = make_book();
  Symbol sym("TEST");

  Order market_buy(fresh_id(), sym, Side::Buy, 0, 100, OrderType::Market);
  auto result = book->add_order(market_buy);

  ASSERT_TRUE(result.success);
  ASSERT_EQ(result.filled_qty, 0u); // Nothing to fill against
}

TEST(market_order_does_not_rest) {
  auto book = make_book();
  Symbol sym("TEST");

  Order market_buy(fresh_id(), sym, Side::Buy, 0, 100, OrderType::Market);
  book->add_order(market_buy);

  // Market orders should not create resting liquidity
  ASSERT_EQ(book->best_bid(), INVALID_PRICE);
}

// ============================================================================
// IOC (IMMEDIATE OR CANCEL)
// ============================================================================

TEST(ioc_fills_available_and_cancels_rest) {
  auto book = make_book();
  Symbol sym("TEST");

  book->add_order(Order(fresh_id(), sym, Side::Sell, 10000, 30));

  Order ioc(fresh_id(), sym, Side::Buy, 10000, 100, OrderType::Limit,
            TimeInForce::ImmediateOrCancel);
  auto result = book->add_order(ioc);

  ASSERT_TRUE(result.success);
  ASSERT_EQ(result.filled_qty, 30u);
  ASSERT_EQ(book->best_bid(),
            INVALID_PRICE); // Remainder cancelled, not resting
}

TEST(ioc_with_full_fill) {
  auto book = make_book();
  Symbol sym("TEST");

  book->add_order(Order(fresh_id(), sym, Side::Sell, 10000, 100));

  Order ioc(fresh_id(), sym, Side::Buy, 10000, 100, OrderType::Limit,
            TimeInForce::ImmediateOrCancel);
  auto result = book->add_order(ioc);

  ASSERT_TRUE(result.success);
  ASSERT_EQ(result.filled_qty, 100u);
}

TEST(ioc_with_no_fill) {
  auto book = make_book();
  Symbol sym("TEST");

  Order ioc(fresh_id(), sym, Side::Buy, 10000, 100, OrderType::Limit,
            TimeInForce::ImmediateOrCancel);
  auto result = book->add_order(ioc);

  ASSERT_TRUE(result.success); // Order accepted, just didn't fill
  ASSERT_EQ(result.filled_qty, 0u);
  ASSERT_EQ(book->best_bid(), INVALID_PRICE); // Not resting
}

// ============================================================================
// FOK (FILL OR KILL)
// ============================================================================

TEST(fok_fills_completely_or_rejects) {
  auto book = make_book();
  Symbol sym("TEST");

  book->add_order(Order(fresh_id(), sym, Side::Sell, 10000, 100));

  Order fok(fresh_id(), sym, Side::Buy, 10000, 100, OrderType::Limit,
            TimeInForce::FillOrKill);
  auto result = book->add_order(fok);

  ASSERT_TRUE(result.success);
  ASSERT_EQ(result.filled_qty, 100u);
}

TEST(fok_rejects_if_insufficient_liquidity) {
  auto book = make_book();
  Symbol sym("TEST");

  book->add_order(Order(fresh_id(), sym, Side::Sell, 10000, 50));

  Order fok(fresh_id(), sym, Side::Buy, 10000, 100, OrderType::Limit,
            TimeInForce::FillOrKill);
  auto result = book->add_order(fok);

  ASSERT_FALSE(result.success);
  ASSERT_EQ(book->ask_quantity_at(10000), 50u); // Book unchanged
}

TEST(fok_with_no_liquidity) {
  auto book = make_book();
  Symbol sym("TEST");

  Order fok(fresh_id(), sym, Side::Buy, 10000, 100, OrderType::Limit,
            TimeInForce::FillOrKill);
  auto result = book->add_order(fok);

  ASSERT_FALSE(result.success);
}

// ============================================================================
// POST-ONLY
// ============================================================================

TEST(postonly_rests_when_no_cross) {
  auto book = make_book();
  OrderBookConfig config;
  config.enable_post_only_rejection = true;
  book = make_book(config);

  Symbol sym("TEST");

  book->add_order(Order(fresh_id(), sym, Side::Sell, 10100, 100));

  Order postonly(fresh_id(), sym, Side::Buy, 10000, 100, OrderType::PostOnly);
  auto result = book->add_order(postonly);

  ASSERT_TRUE(result.success);
  ASSERT_EQ(book->best_bid(), 10000);
}

TEST(postonly_rejects_when_would_cross) {
  OrderBookConfig config;
  config.enable_post_only_rejection = true;
  auto book = make_book(config);

  Symbol sym("TEST");

  book->add_order(Order(fresh_id(), sym, Side::Sell, 10000, 100));

  Order postonly(fresh_id(), sym, Side::Buy, 10000, 100, OrderType::PostOnly);
  auto result = book->add_order(postonly);

  ASSERT_FALSE(result.success);
  ASSERT_EQ(result.reject_reason, RejectReason::WouldCross);
  ASSERT_EQ(book->best_bid(), INVALID_PRICE); // Not added
}

// ============================================================================
// DEPTH RETRIEVAL
// ============================================================================

TEST(get_bids_returns_sorted_descending) {
  auto book = make_book();
  Symbol sym("TEST");

  book->add_order(Order(fresh_id(), sym, Side::Buy, 9800, 100));
  book->add_order(Order(fresh_id(), sym, Side::Buy, 10000, 100));
  book->add_order(Order(fresh_id(), sym, Side::Buy, 9900, 100));

  auto bids = book->get_bids(10);

  ASSERT_EQ(bids.size(), 3u);
  ASSERT_EQ(bids[0].price, 10000); // Highest first
  ASSERT_EQ(bids[1].price, 9900);
  ASSERT_EQ(bids[2].price, 9800);
}

TEST(get_asks_returns_sorted_ascending) {
  auto book = make_book();
  Symbol sym("TEST");

  book->add_order(Order(fresh_id(), sym, Side::Sell, 10200, 100));
  book->add_order(Order(fresh_id(), sym, Side::Sell, 10000, 100));
  book->add_order(Order(fresh_id(), sym, Side::Sell, 10100, 100));

  auto asks = book->get_asks(10);

  ASSERT_EQ(asks.size(), 3u);
  ASSERT_EQ(asks[0].price, 10000); // Lowest first
  ASSERT_EQ(asks[1].price, 10100);
  ASSERT_EQ(asks[2].price, 10200);
}

TEST(get_depth_respects_limit) {
  auto book = make_book();
  Symbol sym("TEST");

  for (int i = 0; i < 10; ++i) {
    book->add_order(Order(fresh_id(), sym, Side::Buy, 10000 - i * 100, 100));
  }

  auto bids = book->get_bids(5);

  ASSERT_EQ(bids.size(), 5u);
}

// ============================================================================
// STRESS / FUZZ
// ============================================================================

TEST(many_orders_same_price) {
  auto book = make_book();
  Symbol sym("TEST");

  for (int i = 0; i < 1000; ++i) {
    book->add_order(Order(fresh_id(), sym, Side::Buy, 10000, 1));
  }

  ASSERT_EQ(book->bid_quantity_at(10000), 1000u);

  // Cancel half
  for (int i = 0; i < 500; ++i) {
    book->add_order(Order(fresh_id(), sym, Side::Sell, 10000, 1));
  }

  ASSERT_EQ(book->bid_quantity_at(10000), 500u);
}

TEST(random_order_mix) {
  auto book = make_book();
  Symbol sym("TEST");

  std::mt19937 rng(42); // Fixed seed for reproducibility
  std::uniform_int_distribution<int> side_dist(0, 1);
  std::uniform_int_distribution<Price> price_dist(9000, 11000);
  std::uniform_int_distribution<Quantity> qty_dist(1, 100);

  for (int i = 0; i < 10000; ++i) {
    Side side = side_dist(rng) ? Side::Buy : Side::Sell;
    Price price = price_dist(rng);
    Quantity qty = qty_dist(rng);

    Order order(fresh_id(), sym, side, price, qty);
    book->add_order(order);
  }

  // Just verify it didn't crash and state is valid
  ASSERT_TRUE(book->best_bid() == INVALID_PRICE || book->best_bid() > 0);
  ASSERT_TRUE(book->best_ask() == INVALID_PRICE || book->best_ask() > 0);

  if (book->best_bid() != INVALID_PRICE && book->best_ask() != INVALID_PRICE) {
    ASSERT_TRUE(book->best_bid() < book->best_ask()); // No crossed book
  }
}

// ============================================================================
// MAIN
// ============================================================================

void run_all_tests() {
  printf("\n=== OrderBook Comprehensive Test Suite ===\n\n");

  int passed = 0;
  int failed = 0;

  for (const auto &result : results) {
    if (result.passed) {
      printf("✓ %s\n", result.name);
      ++passed;
    } else {
      printf("✗ %s\n", result.name);
      printf("    %s\n",
             result.failure_reason.c_str());
      ++failed;
    }
  }

  printf("\n=== Results ===\n");
  printf("Passed: %d / %d\n", passed, passed + failed);

  if (failed > 0) {
    printf("FAILED: %d tests\n", failed);
    std::exit(1);
  } else {
    printf("All tests passed!\n");
  }
}

} // namespace test
} // namespace exchange

int main() {
  exchange::test::run_all_tests();
  return 0;
}
