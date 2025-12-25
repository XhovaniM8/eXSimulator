#include "replay/event_journal.hpp"
#include "utils/timing.hpp"

#include <cstdio>

namespace exchange {

// Simple CRC32 implementation
static uint32_t crc32_table[256];
static bool crc32_initialized = false;

static void init_crc32_table() {
    if (crc32_initialized) return;
    
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int j = 0; j < 8; ++j) {
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }
    crc32_initialized = true;
}

EventJournal::EventJournal(const std::string& path)
    : path_(path), sequence_(0)
{
    init_crc32_table();
    
    file_.open(path, std::ios::binary | std::ios::out | std::ios::app);
    if (!file_) {
        // Handle error
    }
    
    buffer_.reserve(1024);
}

EventJournal::~EventJournal() {
    close();
}

void EventJournal::log_order(const Order& order, EventType type) {
    JournalOrder jo;
    jo.id = order.id;
    jo.symbol = order.symbol;
    jo.side = order.side;
    jo.type = order.type;
    jo.tif = order.tif;
    jo._pad = 0;
    jo.price = order.price;
    jo.quantity = order.quantity;
    jo.timestamp = order.timestamp;
    
    write_header(type, sizeof(JournalOrder));
    file_.write(reinterpret_cast<const char*>(&jo), sizeof(jo));
}

void EventJournal::log_trade(const Trade& trade) {
    JournalTrade jt;
    jt.seq_num = trade.seq_num;
    jt.symbol = trade.symbol;
    jt.price = trade.price;
    jt.quantity = trade.quantity;
    jt.buy_order_id = trade.buy_order_id;
    jt.sell_order_id = trade.sell_order_id;
    jt.aggressor_side = trade.aggressor_side;
    std::memset(jt._pad, 0, sizeof(jt._pad));
    
    write_header(EventType::Trade, sizeof(JournalTrade));
    file_.write(reinterpret_cast<const char*>(&jt), sizeof(jt));
}

void EventJournal::write_header(EventType type, uint32_t payload_size) {
    JournalHeader header;
    header.sequence = sequence_++;
    header.timestamp = Timing::now_ns();
    header.event_type = type;
    std::memset(header.reserved, 0, sizeof(header.reserved));
    header.payload_size = payload_size;
    header.checksum = 0;  // TODO: compute actual checksum
    
    file_.write(reinterpret_cast<const char*>(&header), sizeof(header));
}

uint32_t EventJournal::compute_checksum(const void* data, size_t size) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < size; ++i) {
        crc = crc32_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ 0xFFFFFFFF;
}

void EventJournal::flush() {
    file_.flush();
}

void EventJournal::sync() {
    file_.flush();
    // TODO: call fsync on underlying fd
}

size_t EventJournal::size() const {
    // TODO: return file size
    return 0;
}

void EventJournal::close() {
    if (file_.is_open()) {
        file_.close();
    }
}

// JournalReader implementation
JournalReader::JournalReader(const std::string& path)
    : path_(path), last_valid_(false)
{
    file_.open(path, std::ios::binary | std::ios::in);
}

JournalReader::~JournalReader() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool JournalReader::read_next(JournalHeader& header, 
                              std::vector<uint8_t>& payload) {
    if (!file_ || !file_.is_open()) {
        return false;
    }
    
    // Read header
    file_.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!file_ || file_.gcount() != sizeof(header)) {
        last_valid_ = false;
        return false;
    }
    
    // Read payload
    payload.resize(header.payload_size);
    file_.read(reinterpret_cast<char*>(payload.data()), header.payload_size);
    if (!file_ || static_cast<uint32_t>(file_.gcount()) != header.payload_size) {
        last_valid_ = false;
        return false;
    }
    
    last_header_ = header;
    last_valid_ = true;
    return true;
}

bool JournalReader::seek_to_sequence(uint64_t seq) {
    reset();
    
    JournalHeader header;
    std::vector<uint8_t> payload;
    
    while (read_next(header, payload)) {
        if (header.sequence >= seq) {
            // Seek back to start of this entry
            std::streamoff offset = sizeof(header) + header.payload_size;
            file_.seekg(-offset, std::ios::cur);
            return true;
        }
    }
    
    return false;
}

void JournalReader::reset() {
    file_.clear();
    file_.seekg(0, std::ios::beg);
    last_valid_ = false;
}

bool JournalReader::has_more() const {
    return file_ && file_.peek() != EOF;
}

bool JournalReader::verify_checksum() const {
    // TODO: implement checksum verification
    return last_valid_;
}

uint64_t JournalReader::position() const {
    return static_cast<uint64_t>(
        const_cast<std::ifstream&>(file_).tellg());
}

// ReplayHarness implementation
ReplayHarness::ReplayHarness() : current_seq_(0) {}

ReplayHarness::~ReplayHarness() = default;

void ReplayHarness::load_journal(const std::string& path) {
    reader_ = std::make_unique<JournalReader>(path);
    current_seq_ = 0;
}

void ReplayHarness::replay_all() {
    if (!reader_) return;
    
    while (step()) {
        // Continue until no more events
    }
}

void ReplayHarness::replay_until(uint64_t seq) {
    if (!reader_) return;
    
    while (current_seq_ < seq && step()) {
        // Continue until target sequence
    }
}

bool ReplayHarness::step() {
    if (!reader_) return false;
    
    JournalHeader header;
    std::vector<uint8_t> payload;
    
    if (!reader_->read_next(header, payload)) {
        return false;
    }
    
    current_seq_ = header.sequence;
    
    switch (header.event_type) {
        case EventType::OrderNew:
        case EventType::OrderCancel:
        case EventType::OrderReplace:
            if (on_order_ && payload.size() >= sizeof(JournalOrder)) {
                JournalOrder jo;
                std::memcpy(&jo, payload.data(), sizeof(jo));
                on_order_(jo, header.event_type);
            }
            break;
            
        case EventType::Trade:
            if (on_trade_ && payload.size() >= sizeof(JournalTrade)) {
                JournalTrade jt;
                std::memcpy(&jt, payload.data(), sizeof(jt));
                on_trade_(jt);
            }
            break;
            
        default:
            break;
    }
    
    return true;
}

bool ReplayHarness::verify_against(const std::string& expected_path) {
    // TODO: implement verification
    return true;
}

}  // namespace exchange
