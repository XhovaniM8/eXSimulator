// src/tools/replay_kraken.cpp
//
// Reads a binary replay file produced by scripts/parse_feed.py
// and replays it through the matching engine, measuring throughput.
//
// Build: add to CMakeLists.txt or compile manually:
//   g++ -O2 -std=c++20 -Iinclude src/tools/replay_kraken.cpp \
//       build/libexchange_engine.a build/libexchange_core.a
//       build/libexchange_utils.a \ -o build/replay_kraken

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "core/types.hpp"
#include "engine/matching_engine.hpp"

namespace {

// Must match parse_feed.py CMD_FMT exactly
#pragma pack(push, 1)
struct ReplayCmd {
  uint8_t type; // 0=ADD, 1=CANCEL, 2=REPLACE
  uint8_t side; // 0=BUY, 1=SELL
  uint16_t _pad;
  uint64_t order_id;
  int64_t price;
  uint32_t quantity;
  uint32_t _pad2;
};
#pragma pack(pop)

static_assert(sizeof(ReplayCmd) == 28, "ReplayCmd must be 28 bytes");

enum CmdType : uint8_t { ADD = 0, CANCEL = 1, REPLACE = 2 };

} // namespace

int main(int argc, char *argv[]) {
  const char *path = "data/kraken_replay.bin";
  if (argc > 1)
    path = argv[1];

  // --- Load binary file ---
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    std::cerr << "Error: cannot open " << path << "\n";
    return 1;
  }

  // Read header
  char magic[4];
  uint32_t cmd_count;
  f.read(magic, 4);
  f.read(reinterpret_cast<char *>(&cmd_count), 4);

  if (std::memcmp(magic, "KRKN", 4) != 0) {
    std::cerr << "Error: invalid magic bytes\n";
    return 1;
  }

  std::vector<ReplayCmd> commands(cmd_count);
  f.read(reinterpret_cast<char *>(commands.data()),
         cmd_count * sizeof(ReplayCmd));
  f.close();

  std::cout << "Kraken Replay Benchmark\n";
  std::cout << "=======================\n";
  std::cout << "File:     " << path << "\n";
  std::cout << "Commands: " << cmd_count << "\n\n";

  // --- Set up engine ---
  exchange::EngineConfig config;
  exchange::MatchingEngine engine(config);

  exchange::Symbol sym("XBTUSD");
  engine.add_symbol(sym);

  // --- Count command types ---
  uint32_t n_add = 0, n_cancel = 0, n_replace = 0, n_err = 0;

  // --- Warm up (one pass, don't measure) ---
  for (const auto &cmd : commands) {
    exchange::Order order;
    order.id = cmd.order_id;
    order.symbol = sym;
    order.side = cmd.side == 0 ? exchange::Side::Buy : exchange::Side::Sell;
    order.price = cmd.price;
    order.quantity = cmd.quantity > 0 ? cmd.quantity : 1;
    order.type = exchange::OrderType::Limit;
    order.tif = exchange::TimeInForce::GoodTillCancel;

    if (cmd.type == ADD) {
      engine.submit_order(order);
    } else if (cmd.type == CANCEL) {
      engine.cancel_order(sym, cmd.order_id);
    } else if (cmd.type == REPLACE) {
      engine.replace_order(sym, cmd.order_id, cmd.price, cmd.quantity);
    }
  }

  // Reset for actual benchmark
  // Re-create engine to clear state
  exchange::MatchingEngine engine2(config);
  engine2.add_symbol(sym);

  // --- Timed replay ---
  auto t_start = std::chrono::high_resolution_clock::now();

  for (const auto &cmd : commands) {
    exchange::Order order;
    order.id = cmd.order_id;
    order.symbol = sym;
    order.side = cmd.side == 0 ? exchange::Side::Buy : exchange::Side::Sell;
    order.price = cmd.price;
    order.quantity = cmd.quantity > 0 ? cmd.quantity : 1;
    order.type = exchange::OrderType::Limit;
    order.tif = exchange::TimeInForce::GoodTillCancel;

    exchange::OrderResult result;
    if (cmd.type == ADD) {
      result = engine2.submit_order(order);
      ++n_add;
    } else if (cmd.type == CANCEL) {
      result = engine2.cancel_order(sym, cmd.order_id);
      ++n_cancel;
    } else if (cmd.type == REPLACE) {
      result =
          engine2.replace_order(sym, cmd.order_id, cmd.price, cmd.quantity);
      ++n_replace;
    }

    if (!result.success)
      ++n_err;
  }

  auto t_end = std::chrono::high_resolution_clock::now();
  double elapsed_us =
      std::chrono::duration<double, std::micro>(t_end - t_start).count();
  double elapsed_ms = elapsed_us / 1000.0;
  double ops_per_sec = cmd_count / (elapsed_us / 1e6);
  double ns_per_op = elapsed_us * 1000.0 / cmd_count;

  // --- Results ---
  std::cout << "Command breakdown:\n";
  std::cout << "  ADD:     " << n_add << "\n";
  std::cout << "  CANCEL:  " << n_cancel << "\n";
  std::cout << "  REPLACE: " << n_replace << "\n";
  std::cout << "  Errors:  " << n_err << "\n\n";

  std::cout << "Results\n";
  std::cout << "-------\n";
  std::printf("Elapsed:         %.3f ms\n", elapsed_ms);
  std::printf("Throughput:      %.2f M ops/sec\n", ops_per_sec / 1e6);
  std::printf("Latency/op:      %.1f ns\n", ns_per_op);
  std::cout << "\n";
  std::printf("Real market rate:  ~110 ops/sec (Kraken BTC/USD, 10 levels)\n");
  std::printf("Engine capacity:   %.0fx faster than market\n",
              ops_per_sec / 110.0);

  return 0;
}
