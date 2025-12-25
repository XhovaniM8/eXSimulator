#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "agents/agents.hpp"
#include "core/order.hpp"
#include "engine/matching_engine.hpp"
#include "replay/event_journal.hpp"
#include "utils/histogram.hpp"
#include "utils/timing.hpp"

using namespace exchange;

// Simple command-line argument parsing
struct Config {
    size_t num_symbols = 10;
    size_t num_agents = 100;
    size_t num_ticks = 100000;
    bool enable_journal = false;
    std::string journal_path = "events.journal";
    bool verbose = false;
};

void print_usage(const char* prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --symbols N      Number of symbols (default: 10)\n");
    printf("  --agents N       Number of agents per symbol (default: 100)\n");
    printf("  --ticks N        Number of simulation ticks (default: 100000)\n");
    printf("  --journal PATH   Enable journaling to PATH\n");
    printf("  --verbose        Print progress\n");
    printf("  --help           Show this help\n");
}

Config parse_args(int argc, char** argv) {
    Config config;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            exit(0);
        } else if (arg == "--symbols" && i + 1 < argc) {
            config.num_symbols = std::stoul(argv[++i]);
        } else if (arg == "--agents" && i + 1 < argc) {
            config.num_agents = std::stoul(argv[++i]);
        } else if (arg == "--ticks" && i + 1 < argc) {
            config.num_ticks = std::stoul(argv[++i]);
        } else if (arg == "--journal" && i + 1 < argc) {
            config.enable_journal = true;
            config.journal_path = argv[++i];
        } else if (arg == "--verbose") {
            config.verbose = true;
        }
    }
    
    return config;
}

int main(int argc, char** argv) {
    printf("Exchange Simulator v0.1.0\n");
    printf("=========================\n\n");
    
    Config config = parse_args(argc, argv);
    
    // Calibrate timing
    printf("Calibrating timing...\n");
    Timing::calibrate();
    printf("  TSC frequency: %.2f GHz\n\n", Timing::cycles_per_ns());
    
    // Create engine
    EngineConfig engine_config;
    engine_config.num_symbols = config.num_symbols;
    MatchingEngine engine(engine_config);
    
    // Add symbols
    printf("Adding %zu symbols...\n", config.num_symbols);
    std::vector<Symbol> symbols;
    for (size_t i = 0; i < config.num_symbols; ++i) {
        char name[8];
        snprintf(name, sizeof(name), "SYM%04zu", i);
        Symbol sym(name);
        engine.add_symbol(sym);
        symbols.push_back(sym);
    }
    
    // Create agents
    printf("Creating %zu agents per symbol...\n", config.num_agents);
    std::vector<std::unique_ptr<Agent>> agents;
    
    for (size_t s = 0; s < config.num_symbols; ++s) {
        // Market makers (20% of agents)
        for (size_t i = 0; i < config.num_agents / 5; ++i) {
            MarketMakerConfig mm_config;
            mm_config.symbol = symbols[s];
            mm_config.seed = static_cast<unsigned int>(s * 1000 + i);
            agents.push_back(create_market_maker(mm_config));
        }
        
        // Momentum (20% of agents)
        for (size_t i = 0; i < config.num_agents / 5; ++i) {
            MomentumConfig mom_config;
            mom_config.symbol = symbols[s];
            mom_config.seed = static_cast<unsigned int>(s * 1000 + 200 + i);
            agents.push_back(create_momentum(mom_config));
        }
        
        // Noise traders (60% of agents)
        for (size_t i = 0; i < config.num_agents * 3 / 5; ++i) {
            NoiseTraderConfig noise_config;
            noise_config.symbol = symbols[s];
            noise_config.seed = static_cast<unsigned int>(s * 1000 + 400 + i);
            agents.push_back(create_noise_trader(noise_config));
        }
    }
    printf("  Total agents: %zu\n\n", agents.size());
    
    // Set up latency histogram
    LatencyHistogram latency_hist;
    
    // Trade callback for stats
    uint64_t trade_count = 0;
    uint64_t total_volume = 0;
    engine.set_trade_callback([&](const Trade& trade) {
        ++trade_count;
        total_volume += trade.quantity;
    });
    
    // Run simulation
    printf("Running simulation (%zu ticks)...\n", config.num_ticks);
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (size_t tick = 0; tick < config.num_ticks; ++tick) {
        // Measure tick latency
        uint64_t tick_start = Timing::rdtsc();
        
        // Run all agents
        for (auto& agent : agents) {
            agent->on_tick(tick, engine);
        }
        
        // Process any queued messages
        engine.process_queue();
        
        uint64_t tick_end = Timing::rdtsc();
        uint64_t tick_ns = Timing::cycles_to_ns(tick_end - tick_start);
        latency_hist.record(tick_ns);
        
        // Progress report
        if (config.verbose && (tick + 1) % 10000 == 0) {
            printf("  Tick %zu/%zu (%.1f%%)\n", 
                   tick + 1, config.num_ticks,
                   100.0 * (tick + 1) / config.num_ticks);
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    // Print results
    printf("\nResults\n");
    printf("-------\n");
    printf("Duration:     %ld ms\n", duration_ms);
    printf("Ticks/sec:    %.0f\n", 1000.0 * config.num_ticks / duration_ms);
    printf("Trades:       %lu\n", trade_count);
    printf("Volume:       %lu\n", total_volume);
    printf("Orders:       %lu\n", engine.stats().orders_received);
    printf("Orders/sec:   %.0f\n", 
           1000.0 * engine.stats().orders_received / duration_ms);
    
    printf("\nTick Latency (ns)\n");
    printf("-----------------\n");
    printf("Min:    %lu\n", latency_hist.min());
    printf("p50:    %lu\n", latency_hist.p50());
    printf("p90:    %lu\n", latency_hist.p90());
    printf("p99:    %lu\n", latency_hist.p99());
    printf("p99.9:  %lu\n", latency_hist.p999());
    printf("Max:    %lu\n", latency_hist.max());
    printf("Mean:   %.1f\n", latency_hist.mean());
    
    return 0;
}
