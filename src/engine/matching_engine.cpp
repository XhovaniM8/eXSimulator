#include "engine/matching_engine.hpp"

namespace exchange {

MatchingEngine::MatchingEngine(const EngineConfig& config)
    : config_(config)
    , stats_{}
    , running_(false)
{
    inbound_queue_ = std::make_unique<InboundQueue>();
}

MatchingEngine::~MatchingEngine() {
    stop();
}

void MatchingEngine::add_symbol(const Symbol& symbol) {
    auto book = std::make_unique<OrderBook>(symbol, config_.book_config);
    
    // Set up callbacks
    if (trade_callback_) {
        book->set_trade_callback(trade_callback_);
    }
    if (execution_callback_) {
        book->set_execution_callback(execution_callback_);
    }
    
    symbol_to_book_[symbol] = book.get();
    books_[symbol] = std::move(book);
}

void MatchingEngine::add_symbol(const char* symbol) {
    add_symbol(Symbol(symbol));
}

OrderResult MatchingEngine::submit_order(Order order) {
    ++stats_.orders_received;
    
    OrderBook* book = get_book(order.symbol);
    if (!book) {
        OrderResult result;
        result.success = false;
        result.reject_reason = RejectReason::UnknownSymbol;
        return result;
    }
    
    auto result = book->add_order(order);
    
    if (result.success && result.filled_qty > 0) {
        ++stats_.orders_matched;
        stats_.trades_executed += (result.filled_qty > 0 ? 1 : 0);
        stats_.total_volume += result.filled_qty;
    }
    
    return result;
}

OrderResult MatchingEngine::cancel_order(const Symbol& symbol, OrderId order_id) {
    OrderBook* book = get_book(symbol);
    if (!book) {
        OrderResult result;
        result.success = false;
        result.reject_reason = RejectReason::UnknownSymbol;
        return result;
    }
    
    auto result = book->cancel_order(order_id);
    if (result.success) {
        ++stats_.orders_cancelled;
    }
    
    return result;
}

OrderResult MatchingEngine::replace_order(const Symbol& symbol, OrderId order_id,
                                         Price new_price, Quantity new_qty) {
    OrderBook* book = get_book(symbol);
    if (!book) {
        OrderResult result;
        result.success = false;
        result.reject_reason = RejectReason::UnknownSymbol;
        return result;
    }
    
    return book->replace_order(order_id, new_price, new_qty);
}

bool MatchingEngine::enqueue_order(Order order) {
    InboundMessage msg;
    msg.type = MessageType::NewOrder;
    msg.new_order.order = order;
    return inbound_queue_->try_push(msg);
}

bool MatchingEngine::enqueue_cancel(const Symbol& symbol, OrderId order_id) {
    InboundMessage msg;
    msg.type = MessageType::CancelOrder;
    msg.cancel.order_id = order_id;
    msg.cancel.symbol = symbol;
    return inbound_queue_->try_push(msg);
}

bool MatchingEngine::enqueue_replace(const Symbol& symbol, OrderId order_id,
                                    Price new_price, Quantity new_qty) {
    InboundMessage msg;
    msg.type = MessageType::ReplaceOrder;
    msg.replace.order_id = order_id;
    msg.replace.symbol = symbol;
    msg.replace.new_price = new_price;
    msg.replace.new_qty = new_qty;
    return inbound_queue_->try_push(msg);
}

size_t MatchingEngine::process_queue() {
    size_t processed = 0;
    while (process_one()) {
        ++processed;
    }
    return processed;
}

bool MatchingEngine::process_one() {
    InboundMessage msg;
    if (!inbound_queue_->try_pop(msg)) {
        return false;
    }
    
    ++stats_.messages_processed;
    
    switch (msg.type) {
        case MessageType::NewOrder:
            submit_order(msg.new_order.order);
            break;
        case MessageType::CancelOrder:
            cancel_order(msg.cancel.symbol, msg.cancel.order_id);
            break;
        case MessageType::ReplaceOrder:
            replace_order(msg.replace.symbol, msg.replace.order_id,
                         msg.replace.new_price, msg.replace.new_qty);
            break;
    }
    
    return true;
}

void MatchingEngine::run() {
    running_ = true;
    while (running_) {
        if (!process_one()) {
            // Spin wait or use exponential backoff
            // For now, just spin
        }
    }
}

void MatchingEngine::stop() {
    running_ = false;
}

OrderBook* MatchingEngine::get_book(const Symbol& symbol) {
    auto it = symbol_to_book_.find(symbol);
    return (it != symbol_to_book_.end()) ? it->second : nullptr;
}

const OrderBook* MatchingEngine::get_book(const Symbol& symbol) const {
    auto it = symbol_to_book_.find(symbol);
    return (it != symbol_to_book_.end()) ? it->second : nullptr;
}

void MatchingEngine::reset_stats() {
    stats_ = {};
}

void MatchingEngine::set_trade_callback(TradeCallback cb) {
    trade_callback_ = std::move(cb);
    for (auto& [sym, book] : books_) {
        book->set_trade_callback(trade_callback_);
    }
}

void MatchingEngine::set_execution_callback(ExecutionCallback cb) {
    execution_callback_ = std::move(cb);
    for (auto& [sym, book] : books_) {
        book->set_execution_callback(execution_callback_);
    }
}

// EngineRunner implementation
EngineRunner::EngineRunner(MatchingEngine& engine)
    : engine_(engine), running_(false) {}

EngineRunner::~EngineRunner() {
    stop();
}

void EngineRunner::start() {
    running_ = true;
    // TODO: Start thread
}

void EngineRunner::stop() {
    running_ = false;
    engine_.stop();
    // TODO: Join thread
}

bool EngineRunner::is_running() const {
    return running_;
}

}  // namespace exchange
