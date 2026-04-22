// quick test for market orders, PostOnly, IOC/FOK, self-trade prevention
#include "core/types.hpp"
#include "engine/matching_engine.hpp"
#include <cinttypes>
#include <cstdio>

using namespace exchange;

int main() {
  printf("=== Feature Test ===\n\n");

  MatchingEngine engine;
  Symbol symbol("TEST");
  engine.add_symbol(symbol);

  OrderId next_id = 1;

  // Test 1: basic matching
  printf("Test 1: Limit matching\n");
  {
    Order buy(next_id++, symbol, Side::Buy, 10000, 100);
    Order sell(next_id++, symbol, Side::Sell, 10000, 50);

    engine.submit_order(buy);
    auto result = engine.submit_order(sell);

    printf("filled: %u\n", result.filled_qty);
  }

  // Test 2: market order
  printf("\nTest 2: Market order\n");
  {
    Order limit_ask(next_id++, symbol, Side::Sell, 10100, 100);
    engine.submit_order(limit_ask);

    Order market_buy(next_id++, symbol, Side::Buy, 0, 50, OrderType::Market);
    auto result = engine.submit_order(market_buy);

    printf("Market filled: %u @ $%.4f\n", result.filled_qty,
           price_to_double(10100));
  }

  // Test 3: PostOnly
  printf("\nTest 3: PostOnly (rejects if crosses)\n");
  {
    Order limit_bid(next_id++, symbol, Side::Buy, 10000, 100);
    engine.submit_order(limit_bid);

    Order postonly_sell(next_id++, symbol, Side::Sell, 9900, 50,
                        OrderType::PostOnly);
    auto result = engine.submit_order(postonly_sell);

    if (!result.success && result.reject_reason == RejectReason::WouldCross) {
      printf("rejected (would cross) ✓\n");
    } else {
      printf("should have been rejected\n");
    }
  }

  // Test 4: IOC
  printf("\nTest 4: IOC\n");
  {
    Order limit_ask2(next_id++, symbol, Side::Sell, 10200, 30);
    engine.submit_order(limit_ask2);

    Order ioc_buy(next_id++, symbol, Side::Buy, 10200, 100,
                  OrderType::ImmediateOrCancel);
    auto result = engine.submit_order(ioc_buy);

    printf("filled %u of 100, rest cancelled\n", result.filled_qty);
  }

  // Test 5: FOK
  printf("\nTest 5: FOK\n");
  {
    Order limit_bid2(next_id++, symbol, Side::Buy, 9800, 20);
    engine.submit_order(limit_bid2);

    Order fok_sell(next_id++, symbol, Side::Sell, 9800, 100,
                   OrderType::FillOrKill);
    auto result = engine.submit_order(fok_sell);

    if (!result.success) {
      printf("rejected (not enough liquidity) ✓\n");
    } else {
      printf("should have been rejected\n");
    }
  }

  // Test 6: Self-trade check
  printf("\nTest 6: Self-trade prevention\n");
  {
    Order buy1(next_id++, symbol, Side::Buy, 10000, 100);
    Order sell1(next_id + 10, symbol, Side::Sell, 10000, 50);

    engine.submit_order(buy1);
    auto result = engine.submit_order(sell1);

    if (result.filled_qty == 0) {
      printf("prevented ✓\n");
    } else {
      printf("trade occurred (check prevention logic)\n");
    }
    next_id += 20;
  }

  printf("\n=== Done ===\n");
  printf("\nStats:\n");
  printf("  Orders: %" PRIu64 "\n", engine.stats().orders_received);
  printf("  Trades: %" PRIu64 "\n", engine.stats().trades_executed);
  printf("  Cancels: %" PRIu64 "\n", engine.stats().orders_cancelled);

  return 0;
}
