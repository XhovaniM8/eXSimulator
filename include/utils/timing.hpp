#pragma once

#include <chrono>
#include <cstdint>

namespace exchange {

// High-resolution timing using RDTSC instruction
// Provides sub-nanosecond precision on modern x86 CPUs
// Note: Requires constant_tsc CPU flag for reliable cross-core measurements

class Timing {
public:
    // Read timestamp counter (TSC)
    // Returns CPU cycles, not nanoseconds
    static inline uint64_t rdtsc() {
#if defined(__x86_64__) || defined(_M_X64)
        uint32_t lo, hi;
        __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
        return (static_cast<uint64_t>(hi) << 32) | lo;
#else
        // Fallback for non-x86
        auto now = std::chrono::high_resolution_clock::now();
        return static_cast<uint64_t>(now.time_since_epoch().count());
#endif
    }
    
    // Read TSC with serialization (more accurate but slower)
    // Use for measuring very short code sections
    static inline uint64_t rdtscp() {
#if defined(__x86_64__) || defined(_M_X64)
        uint32_t lo, hi, aux;
        __asm__ volatile("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux));
        return (static_cast<uint64_t>(hi) << 32) | lo;
#else
        return rdtsc();
#endif
    }
    
    // Get current time in nanoseconds since epoch
    static inline uint64_t now_ns() {
        auto now = std::chrono::high_resolution_clock::now();
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count());
    }
    
    // Get current time in microseconds since epoch
    static inline uint64_t now_us() {
        auto now = std::chrono::high_resolution_clock::now();
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count());
    }
    
    // Calibrate TSC to nanoseconds
    // Call once at startup
    static void calibrate() {
        auto t1 = std::chrono::high_resolution_clock::now();
        uint64_t tsc1 = rdtsc();
        
        // Spin for 10ms
        while (std::chrono::high_resolution_clock::now() - t1 < 
               std::chrono::milliseconds(10)) {
            // spin
        }
        
        auto t2 = std::chrono::high_resolution_clock::now();
        uint64_t tsc2 = rdtsc();
        
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
        uint64_t cycles = tsc2 - tsc1;
        
        cycles_per_ns_ = static_cast<double>(cycles) / static_cast<double>(ns);
        ns_per_cycle_ = static_cast<double>(ns) / static_cast<double>(cycles);
    }
    
    // Convert cycles to nanoseconds
    static inline uint64_t cycles_to_ns(uint64_t cycles) {
        return static_cast<uint64_t>(static_cast<double>(cycles) * ns_per_cycle_);
    }
    
    // Convert nanoseconds to cycles
    static inline uint64_t ns_to_cycles(uint64_t ns) {
        return static_cast<uint64_t>(static_cast<double>(ns) * cycles_per_ns_);
    }
    
    // Get calibrated frequency
    static double cycles_per_ns() { return cycles_per_ns_; }
    static double ns_per_cycle() { return ns_per_cycle_; }
    
private:
    static inline double cycles_per_ns_ = 3.0;  // ~3GHz default
    static inline double ns_per_cycle_ = 0.333;
};

// RAII timer for measuring code sections
class ScopedTimer {
public:
    explicit ScopedTimer(uint64_t& output_ns) 
        : output_(output_ns), start_(Timing::rdtsc()) {}
    
    ~ScopedTimer() {
        uint64_t end = Timing::rdtsc();
        output_ = Timing::cycles_to_ns(end - start_);
    }
    
    // Non-copyable
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
    
private:
    uint64_t& output_;
    uint64_t start_;
};

// Stopwatch for manual timing
class Stopwatch {
public:
    void start() { start_ = Timing::rdtsc(); }
    
    void stop() { 
        end_ = Timing::rdtsc(); 
        elapsed_cycles_ += (end_ - start_);
    }
    
    void reset() { elapsed_cycles_ = 0; }
    
    uint64_t elapsed_cycles() const { return elapsed_cycles_; }
    uint64_t elapsed_ns() const { return Timing::cycles_to_ns(elapsed_cycles_); }
    double elapsed_us() const { return static_cast<double>(elapsed_ns()) / 1000.0; }
    double elapsed_ms() const { return static_cast<double>(elapsed_ns()) / 1000000.0; }
    
private:
    uint64_t start_ = 0;
    uint64_t end_ = 0;
    uint64_t elapsed_cycles_ = 0;
};

}  // namespace exchange
