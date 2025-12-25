#include "agents/agents.hpp"
#include "engine/matching_engine.hpp"

namespace exchange {

MomentumAgent::MomentumAgent(const MomentumConfig& config)
    : Agent("MOM_" + std::string(config.symbol.data, SYMBOL_SIZE))
    , config_(config)
    , rng_(config.seed)
    , position_(0)
{
    price_history_.reserve(config.lookback_ticks);
}

MomentumAgent::~MomentumAgent() = default;

void MomentumAgent::on_tick(uint64_t tick, MatchingEngine& engine) {
    if (price_history_.size() < config_.lookback_ticks) {
        return;  // Not enough data
    }
    
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    if (dist(rng_) > config_.trade_probability) {
        return;
    }
    
    double signal = compute_signal();
    
    if (signal > config_.threshold) {
        // Buy signal
        OrderId id = next_order_id();
        Order order(id, config_.symbol, Side::Buy, 0, config_.order_size,
                   OrderType::Market);
        engine.submit_order(order);
    } else if (signal < -config_.threshold) {
        // Sell signal
        OrderId id = next_order_id();
        Order order(id, config_.symbol, Side::Sell, 0, config_.order_size,
                   OrderType::Market);
        engine.submit_order(order);
    }
}

void MomentumAgent::on_fill(const ExecutionReport& report) {
    if (report.side == Side::Buy) {
        position_ += static_cast<int64_t>(report.last_qty);
    } else {
        position_ -= static_cast<int64_t>(report.last_qty);
    }
}

void MomentumAgent::on_bbo(const BBOUpdate& bbo) {
    Price mid = (bbo.bid_price + bbo.ask_price) / 2;
    
    price_history_.push_back(mid);
    if (price_history_.size() > config_.lookback_ticks) {
        price_history_.erase(price_history_.begin());
    }
}

double MomentumAgent::compute_signal() const {
    if (price_history_.size() < 2) {
        return 0.0;
    }
    
    Price first = price_history_.front();
    Price last = price_history_.back();
    
    if (first == 0) {
        return 0.0;
    }
    
    return static_cast<double>(last - first) / static_cast<double>(first);
}

std::unique_ptr<Agent> create_momentum(const MomentumConfig& config) {
    return std::make_unique<MomentumAgent>(config);
}

}  // namespace exchange
