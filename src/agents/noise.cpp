#include "agents/agents.hpp"
#include "engine/matching_engine.hpp"

namespace exchange {

NoiseTrader::NoiseTrader(const NoiseTraderConfig& config)
    : Agent("NOISE_" + std::string(config.symbol.data, SYMBOL_SIZE))
    , config_(config)
    , rng_(config.seed)
    , last_mid_(100 * PRICE_SCALE)  // Default starting price
{}

NoiseTrader::~NoiseTrader() = default;

void NoiseTrader::on_tick(uint64_t tick, MatchingEngine& engine) {
    std::uniform_real_distribution<double> prob_dist(0.0, 1.0);
    
    if (prob_dist(rng_) > config_.order_probability) {
        return;  // No order this tick
    }
    
    // Decide side randomly
    Side side = prob_dist(rng_) < 0.5 ? Side::Buy : Side::Sell;
    
    // Decide size
    std::uniform_int_distribution<Quantity> size_dist(
        config_.min_size, config_.max_size);
    Quantity size = size_dist(rng_);
    
    // Decide order type
    bool is_market = prob_dist(rng_) < config_.market_order_ratio;
    
    OrderId id = next_order_id();
    
    if (is_market) {
        Order order(id, config_.symbol, side, 0, size, OrderType::Market);
        engine.submit_order(order);
    } else {
        // Limit order with random price deviation
        std::uniform_int_distribution<Price> price_dist(
            -config_.max_deviation, config_.max_deviation);
        Price deviation = price_dist(rng_);
        
        Price price;
        if (side == Side::Buy) {
            price = last_mid_ - config_.max_deviation / 2 + deviation;
        } else {
            price = last_mid_ + config_.max_deviation / 2 + deviation;
        }
        
        if (price <= 0) {
            price = 1;  // Minimum valid price
        }
        
        Order order(id, config_.symbol, side, price, size);
        engine.submit_order(order);
    }
}

void NoiseTrader::on_bbo(const BBOUpdate& bbo) {
    if (bbo.bid_price > 0 && bbo.ask_price < INVALID_PRICE) {
        last_mid_ = (bbo.bid_price + bbo.ask_price) / 2;
    }
}

std::unique_ptr<Agent> create_noise_trader(const NoiseTraderConfig& config) {
    return std::make_unique<NoiseTrader>(config);
}

}  // namespace exchange
