#pragma once

#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "core/order.hpp"
#include "core/trade.hpp"

namespace exchange {

// Event types for journal
enum class EventType : uint8_t {
    OrderNew = 0,
    OrderCancel = 1,
    OrderReplace = 2,
    Trade = 3,
    BookSnapshot = 4
};

// Journal entry header (fixed size for easy seeking)
struct JournalHeader {
    uint64_t sequence;       // Monotonic sequence number
    uint64_t timestamp;      // Nanoseconds since epoch
    EventType event_type;
    uint8_t reserved[7];     // Changed from 3 to 7 for proper alignment
    uint32_t payload_size;   // Size of following payload
    uint32_t checksum;       // CRC32 of payload
};

static_assert(sizeof(JournalHeader) == 32, "Header should be 32 bytes");

// Serialized order for journal
struct JournalOrder {
    OrderId id;
    Symbol symbol;
    Side side;
    OrderType type;
    TimeInForce tif;
    uint8_t _pad;
    Price price;
    Quantity quantity;
    uint64_t timestamp;
};

static_assert(sizeof(JournalOrder) == 48, "JournalOrder should be 48 bytes");

// Serialized trade for journal
struct JournalTrade {
    SequenceNum seq_num;
    Symbol symbol;
    Price price;
    Quantity quantity;
    OrderId buy_order_id;
    OrderId sell_order_id;
    Side aggressor_side;
    uint8_t _pad[7];
};

// Event journal for deterministic replay
// Append-only binary log with CRC verification
class EventJournal {
public:
    explicit EventJournal(const std::string& path);
    ~EventJournal();
    
    // Disable copy/move
    EventJournal(const EventJournal&) = delete;
    EventJournal& operator=(const EventJournal&) = delete;
    
    // Append events
    void log_order(const Order& order, EventType type);
    void log_trade(const Trade& trade);
    
    // Force flush to disk
    void flush();
    
    // Sync to disk (fsync)
    void sync();
    
    // Get current sequence number
    uint64_t sequence() const { return sequence_; }
    
    // Get file size
    size_t size() const;
    
    // Close journal
    void close();
    
private:
    void write_header(EventType type, uint32_t payload_size, uint32_t checksum);
    uint32_t compute_checksum(const void* data, size_t size);
    
    std::string path_;
    std::ofstream file_;
    uint64_t sequence_;
    size_t file_size_;
    std::vector<uint8_t> buffer_;  // Write buffer
};

// Journal reader for replay
class JournalReader {
public:
    explicit JournalReader(const std::string& path);
    ~JournalReader();
    
    // Read next event
    // Returns false if end of journal or error
    bool read_next(JournalHeader& header, std::vector<uint8_t>& payload);
    
    // Seek to sequence number
    bool seek_to_sequence(uint64_t seq);
    
    // Reset to beginning
    void reset();
    
    // Check if more events available
    bool has_more() const;
    
    // Verify checksum of last read event
    bool verify_checksum() const;
    
    // Get current position
    uint64_t position() const;
    
private:
    std::string path_;
    std::ifstream file_;
    JournalHeader last_header_;
    std::vector<uint8_t> last_payload_;
    bool last_valid_;
};

// Replay harness for deterministic re-execution
class ReplayHarness {
public:
    ReplayHarness();
    ~ReplayHarness();
    
    // Load journal for replay
    void load_journal(const std::string& path);
    
    // Set callback for each event type
    using OrderCallback = std::function<void(const JournalOrder&, EventType)>;
    using TradeCallback = std::function<void(const JournalTrade&)>;
    
    void set_order_callback(OrderCallback cb) { on_order_ = std::move(cb); }
    void set_trade_callback(TradeCallback cb) { on_trade_ = std::move(cb); }
    
    // Replay all events
    void replay_all();
    
    // Replay events up to sequence number
    void replay_until(uint64_t seq);
    
    // Step through one event at a time
    bool step();
    
    // Get current sequence number
    uint64_t current_sequence() const { return current_seq_; }
    
    // Verify replay produces identical output
    // Compare output journal against expected
    bool verify_against(const std::string& expected_path);
    
private:
    std::unique_ptr<JournalReader> reader_;
    uint64_t current_seq_;
    OrderCallback on_order_;
    TradeCallback on_trade_;
};

}  // namespace exchange
