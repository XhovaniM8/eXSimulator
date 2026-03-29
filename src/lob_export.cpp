// lob_export.cpp
// Runs the exchange simulation and exports LOB snapshots in LOBSTER CSV format.
// Output is compatible with the Python feature extraction pipeline in fpga-orderbook-analysis.
//
// Orderbook CSV columns (5 levels):
//   AskPrice1, AskSize1, BidPrice1, BidSize1, ..., AskPrice5, AskSize5, BidPrice5, BidSize5
// Prices are stored as integer * 10000 (matching LOBSTER convention).
//
// Message CSV columns:
//   Time, Type, OrderID, Size, Price, Direction
// Type 1 = new limit order (all synthetic orders are type 1 here).
//
// Usage:
//   ./lob_export --ticks 200000 --levels 5 --seed 42 --out /path/to/output
//
// This generates:
//   <out>/SYNTHETIC_<date>_34200000_57600000_orderbook_<levels>.csv
//   <out>/SYNTHETIC_<date>_34200000_57600000_message_<levels>.csv

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <limits>

#include "agents/agents.hpp"
#include "core/order.hpp"
#include "core/types.hpp"
#include "engine/matching_engine.hpp"
#include "utils/timing.hpp"

using namespace exchange;

struct ExportConfig {
    size_t num_ticks   = 200000;
    size_t n_levels    = 5;
    unsigned int seed  = 42;
    std::string out_dir = ".";
    // Initial mid price for AAPL-like instrument (in PRICE_SCALE units, ~$585)
    Price init_mid = 5850000;
    bool verbose = false;
};

ExportConfig parse_args(int argc, char** argv) {
    ExportConfig cfg;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if      (a == "--ticks"  && i+1 < argc) cfg.num_ticks  = std::stoul(argv[++i]);
        else if (a == "--levels" && i+1 < argc) cfg.n_levels   = std::stoul(argv[++i]);
        else if (a == "--seed"   && i+1 < argc) cfg.seed       = static_cast<unsigned>(std::stoul(argv[++i]));
        else if (a == "--out"    && i+1 < argc) cfg.out_dir    = argv[++i];
        else if (a == "--mid"    && i+1 < argc) cfg.init_mid   = static_cast<Price>(std::stoll(argv[++i]));
        else if (a == "--verbose")              cfg.verbose    = true;
    }
    return cfg;
}

int main(int argc, char** argv) {
    ExportConfig cfg = parse_args(argc, argv);

    printf("LOB Exporter\n");
    printf("  Ticks:  %zu\n", cfg.num_ticks);
    printf("  Levels: %zu\n", cfg.n_levels);
    printf("  Seed:   %u\n",  cfg.seed);
    printf("  Out:    %s\n\n", cfg.out_dir.c_str());

    // Build output paths (LOBSTER naming convention)
    char ob_path[512], msg_path[512];
    snprintf(ob_path,  sizeof(ob_path),  "%s/SYNTHETIC_2012-06-21_34200000_57600000_orderbook_%zu.csv",
             cfg.out_dir.c_str(), cfg.n_levels);
    snprintf(msg_path, sizeof(msg_path), "%s/SYNTHETIC_2012-06-21_34200000_57600000_message_%zu.csv",
             cfg.out_dir.c_str(), cfg.n_levels);

    std::ofstream ob_file(ob_path);
    std::ofstream msg_file(msg_path);
    if (!ob_file || !msg_file) {
        fprintf(stderr, "Failed to open output files in %s\n", cfg.out_dir.c_str());
        return 1;
    }

    // Single symbol
    Symbol sym("AAPL");

    EngineConfig engine_cfg;
    engine_cfg.num_symbols = 1;
    MatchingEngine engine(engine_cfg);
    engine.add_symbol(sym);

    // Seed the book with initial quotes so agents have a reference price
    const Price half_spread = 500; // $0.05
    const Price init_bid = cfg.init_mid - half_spread;
    const Price init_ask = cfg.init_mid + half_spread;
    {
        engine.submit_order(Order(999000001, sym, Side::Buy,  init_bid, 1000,
                                  OrderType::Limit, TimeInForce::GoodTillCancel));
        engine.submit_order(Order(999000002, sym, Side::Sell, init_ask, 1000,
                                  OrderType::Limit, TimeInForce::GoodTillCancel));
        engine.process_queue();
    }

    // Create agents (same ratio as main.cpp: 20% MM, 20% momentum, 60% noise)
    const size_t n_agents = 30;
    std::vector<std::unique_ptr<Agent>> agents;

    for (size_t i = 0; i < n_agents / 5; ++i) {
        MarketMakerConfig mm;
        mm.symbol       = sym;
        mm.half_spread  = half_spread;
        mm.base_size    = 100;
        mm.seed         = cfg.seed + static_cast<unsigned>(i);
        agents.push_back(create_market_maker(mm));
    }
    for (size_t i = 0; i < n_agents / 5; ++i) {
        MomentumConfig mom;
        mom.symbol = sym;
        mom.seed   = cfg.seed + 200u + static_cast<unsigned>(i);
        agents.push_back(create_momentum(mom));
    }
    for (size_t i = 0; i < n_agents * 3 / 5; ++i) {
        NoiseTraderConfig noise;
        noise.symbol        = sym;
        noise.seed          = cfg.seed + 400u + static_cast<unsigned>(i);
        // Scale deviation to be meaningful relative to the actual mid price.
        // PRICE_SCALE=10000 so 1 tick = $0.01; 1000 ticks = $1 range each side.
        noise.max_deviation = 10000;  // ±$1 around mid — realistic for large-cap
        agents.push_back(create_noise_trader(noise));
    }

    // Wire BBO callback: broadcast to all agents so they learn the correct
    // mid price before tick 0.  The seed orders placed above fire BBO when
    // process_queue() runs, but that happens before agents exist.  Fire a
    // synthetic BBO manually now so every agent starts from init_mid.
    {
        BBOUpdate init_bbo{};
        init_bbo.symbol    = sym;
        init_bbo.bid_price = init_bid;
        init_bbo.bid_qty   = 1000;
        init_bbo.ask_price = init_ask;
        init_bbo.ask_qty   = 1000;
        init_bbo.timestamp = 0;
        for (auto& a : agents)
            a->on_bbo(init_bbo);
    }

    // Wire ongoing BBO callbacks so agents track mid during the simulation.
    {
        OrderBook* bbo_book = engine.get_book(sym);
        if (bbo_book) {
            bbo_book->set_bbo_callback([&agents](const BBOUpdate& bbo) {
                for (auto& a : agents)
                    a->on_bbo(bbo);
            });
        }
    }

    // Dummy sentinel price for empty levels
    constexpr Price EMPTY_ASK = 9999999999LL;
    constexpr Price EMPTY_BID = -9999999999LL;

    // Simulated time starts at 9:30 AM (34200 seconds after midnight)
    double sim_time = 34200.0;
    const double tick_dt = (57600.0 - 34200.0) / static_cast<double>(cfg.num_ticks);

    uint64_t order_seq = 1;
    size_t snapshots_written = 0;

    for (size_t tick = 0; tick < cfg.num_ticks; ++tick) {
        for (auto& agent : agents)
            agent->on_tick(tick, engine);
        engine.process_queue();

        sim_time += tick_dt;

        // Get LOB snapshot via the per-symbol order book
        const OrderBook* snap = engine.get_book(sym);
        auto bids = snap ? snap->get_bids(cfg.n_levels) : std::vector<PriceLevelUpdate>{};
        auto asks = snap ? snap->get_asks(cfg.n_levels) : std::vector<PriceLevelUpdate>{};

        // Write orderbook row: AskP1,AskS1,BidP1,BidS1,...
        for (size_t l = 0; l < cfg.n_levels; ++l) {
            Price ap = (l < asks.size()) ? asks[l].price : EMPTY_ASK;
            long long as_ = (l < asks.size()) ? static_cast<long long>(asks[l].quantity) : 0LL;
            Price bp = (l < bids.size()) ? bids[l].price : EMPTY_BID;
            long long bs = (l < bids.size()) ? static_cast<long long>(bids[l].quantity) : 0LL;
            if (l > 0) ob_file << ',';
            ob_file << ap << ',' << as_ << ',' << bp << ',' << bs;
        }
        ob_file << '\n';

        // Write a message row for this tick (type 1 = new limit, synthetic)
        msg_file << sim_time << ",1," << order_seq++ << ",1,0,1\n";

        ++snapshots_written;
        if (cfg.verbose && snapshots_written % 10000 == 0)
            printf("  Snapshot %zu / %zu\n", snapshots_written, cfg.num_ticks);
    }

    printf("Wrote %zu snapshots\n", snapshots_written);
    printf("  Orderbook: %s\n", ob_path);
    printf("  Messages:  %s\n", msg_path);
    return 0;
}
