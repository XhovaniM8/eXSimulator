#include "agents/agents.hpp"
#include "engine/matching_engine.hpp"

namespace exchange {

MarketMaker::MarketMaker(const MarketMakerConfig& config)
    : Agent("MM_" + std::string(config.symbol.data, SYMBOL_SIZE))
    , config_(config)
    , rng_(config.seed)
    , position_(0)
    , realized_pnl_(0)
    , num_trades_(0)
    , last_mid_(0)
    , bid_order_id_(0)
    , ask_order_id_(0)
{}

MarketMaker::~MarketMaker() = default;

void MarketMaker::on_tick(uint64_t tick, MatchingEngine& engine) {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    
    if (dist(rng_) < config_.quote_probability) {
        update_quotes(engine);
    }
}

void MarketMaker::on_fill(const ExecutionReport& report) {
    if (report.side == Side::Buy) {
        position_ += static_cast<int64_t>(report.last_qty);
    } else {
        position_ -= static_cast<int64_t>(report.last_qty);
    }
    ++num_trades_;
}

void MarketMaker::on_trade(const Trade& trade) {
    // Update fair value based on trades
}

void MarketMaker::on_bbo(const BBOUpdate& bbo) {
    last_mid_ = (bbo.bid_price + bbo.ask_price) / 2;
}

void MarketMaker::update_quotes(MatchingEngine& engine) {
    Price fair = compute_fair_value();
    if (fair == 0) {
        fair = 100 * PRICE_SCALE;  // Default fair value
    }
    
    // Adjust for inventory
    Price skew = static_cast<Price>(position_ * config_.inventory_skew * PRICE_SCALE);
    
    Price bid = fair - config_.half_spread - skew;
    Price ask = fair + config_.half_spread - skew;
    
    // Cancel existing orders
    if (bid_order_id_ > 0) {
        engine.cancel_order(config_.symbol, bid_order_id_);
    }
    if (ask_order_id_ > 0) {
        engine.cancel_order(config_.symbol, ask_order_id_);
    }
    
    // Submit new orders if within position limits
    if (position_ < static_cast<int64_t>(config_.max_position)) {
        bid_order_id_ = next_order_id();
        Order bid_order(bid_order_id_, config_.symbol, Side::Buy, 
                       bid, config_.base_size);
        engine.submit_order(bid_order);
    }
    
    if (position_ > -static_cast<int64_t>(config_.max_position)) {
        ask_order_id_ = next_order_id();
        Order ask_order(ask_order_id_, config_.symbol, Side::Sell,
                       ask, config_.base_size);
        engine.submit_order(ask_order);
    }
}

void MarketMaker::cancel_all(MatchingEngine& engine) {
    if (bid_order_id_ > 0) {
        engine.cancel_order(config_.symbol, bid_order_id_);
        bid_order_id_ = 0;
    }
    if (ask_order_id_ > 0) {
        engine.cancel_order(config_.symbol, ask_order_id_);
        ask_order_id_ = 0;
    }
}

Price MarketMaker::compute_fair_value() const {
    return last_mid_;
}

std::unique_ptr<Agent> create_market_maker(const MarketMakerConfig& config) {
    return std::make_unique<MarketMaker>(config);
}

}  // namespace exchange
