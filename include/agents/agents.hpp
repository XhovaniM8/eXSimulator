#pragma once

#include <functional>
#include <memory>
#include <random>
#include <string>

#include "core/order.hpp"
#include "core/trade.hpp"

namespace exchange {

// Forward declaration
class MatchingEngine;

// Base class for trading agents
class Agent {
public:
    explicit Agent(const std::string& name) : name_(name), next_order_id_(1) {}
    virtual ~Agent() = default;
    
    // Called on each simulation tick
    virtual void on_tick(uint64_t tick, MatchingEngine& engine) = 0;
    
    // Called when agent's order is filled
    virtual void on_fill(const ExecutionReport& report) {}
    
    // Called on trade in market (any trade)
    virtual void on_trade(const Trade& trade) {}
    
    // Called on BBO update
    virtual void on_bbo(const BBOUpdate& bbo) {}
    
    // Get agent name
    const std::string& name() const { return name_; }
    
    // Generate next order ID (agent-unique)
    OrderId next_order_id() { return next_order_id_++; }
    
protected:
    std::string name_;
    OrderId next_order_id_;
};

// Market maker agent
// Quotes two-sided markets around fair value
// Adjusts spreads based on inventory
struct MarketMakerConfig {
    Symbol symbol{"AAPL"};
    Quantity base_size = 100;         // Base quote size
    Price half_spread = 10;           // Half spread in price ticks
    double inventory_skew = 0.1;      // Price adjustment per unit inventory
    Quantity max_position = 1000;     // Max absolute position
    double quote_probability = 0.9;   // Probability of quoting each tick
    unsigned int seed = 42;
};

class MarketMaker : public Agent {
public:
    explicit MarketMaker(const MarketMakerConfig& config);
    ~MarketMaker() override;
    
    void on_tick(uint64_t tick, MatchingEngine& engine) override;
    void on_fill(const ExecutionReport& report) override;
    void on_trade(const Trade& trade) override;
    void on_bbo(const BBOUpdate& bbo) override;
    
    // Statistics
    int64_t position() const { return position_; }
    int64_t pnl() const { return realized_pnl_; }
    uint64_t trades() const { return num_trades_; }
    
private:
    void update_quotes(MatchingEngine& engine);
    void cancel_all(MatchingEngine& engine);
    Price compute_fair_value() const;
    
    MarketMakerConfig config_;
    std::mt19937 rng_;
    
    // State
    int64_t position_;
    int64_t realized_pnl_;
    uint64_t num_trades_;
    Price last_mid_;
    
    // Active orders
    OrderId bid_order_id_;
    OrderId ask_order_id_;
};

// Momentum/trend-following agent
// Trades in direction of recent price movement
struct MomentumConfig {
    Symbol symbol{"AAPL"};
    size_t lookback_ticks = 100;      // Price history length
    double threshold = 0.001;          // Min return to trigger signal
    Quantity order_size = 50;
    double trade_probability = 0.5;
    unsigned int seed = 123;
};

class MomentumAgent : public Agent {
public:
    explicit MomentumAgent(const MomentumConfig& config);
    ~MomentumAgent() override;
    
    void on_tick(uint64_t tick, MatchingEngine& engine) override;
    void on_fill(const ExecutionReport& report) override;
    void on_bbo(const BBOUpdate& bbo) override;
    
    int64_t position() const { return position_; }
    
private:
    double compute_signal() const;
    
    MomentumConfig config_;
    std::mt19937 rng_;
    
    std::vector<Price> price_history_;
    int64_t position_;
};

// Noise trader (random orders for liquidity)
struct NoiseTraderConfig {
    Symbol symbol{"AAPL"};
    double order_probability = 0.1;   // Probability of order each tick
    double market_order_ratio = 0.3;  // Fraction that are market orders
    Quantity min_size = 10;
    Quantity max_size = 100;
    Price max_deviation = 100;        // Max deviation from mid for limits
    unsigned int seed = 456;
};

class NoiseTrader : public Agent {
public:
    explicit NoiseTrader(const NoiseTraderConfig& config);
    ~NoiseTrader() override;
    
    void on_tick(uint64_t tick, MatchingEngine& engine) override;
    void on_bbo(const BBOUpdate& bbo) override;
    
private:
    NoiseTraderConfig config_;
    std::mt19937 rng_;
    Price last_mid_;
};

// Factory for creating agents
std::unique_ptr<Agent> create_market_maker(const MarketMakerConfig& config);
std::unique_ptr<Agent> create_momentum(const MomentumConfig& config);
std::unique_ptr<Agent> create_noise_trader(const NoiseTraderConfig& config);

}  // namespace exchange
