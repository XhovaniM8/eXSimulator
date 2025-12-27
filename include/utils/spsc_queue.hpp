#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <optional>
#include <type_traits>

namespace exchange {

// Portable cache line size definition
// std::hardware_destructive_interference_size is not reliably portable across
// compilers and can cause -Winterference-size warnings with -Werror.
// 64 bytes is correct for x86-64 (Intel/AMD) and most ARM64 processors.
// Apple M1/M2 use 128-byte cache lines but 64 works fine (just suboptimal).
#ifdef __cpp_lib_hardware_interference_size
    // Only use if available AND we're not treating warnings as errors
    // In practice, just use the constant to avoid CI headaches
    constexpr size_t CACHE_LINE_SIZE = 64;
#else
    constexpr size_t CACHE_LINE_SIZE = 64;
#endif

// Single-Producer Single-Consumer lock-free queue
// Based on Rigtorp's design: https://github.com/rigtorp/SPSCQueue
//
// Memory layout:
//   - head_ and cached_tail_ on same cache line (producer's line)
//   - tail_ and cached_head_ on same cache line (consumer's line)
//   - slots_ on separate cache lines
//
// This avoids false sharing between producer and consumer threads.

template <typename T, size_t Capacity>
class SPSCQueue {
    static_assert(Capacity > 0, "Capacity must be positive");
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

public:
    SPSCQueue() : head_(0), cached_tail_(0), tail_(0), cached_head_(0) {
        // Touch all pages to avoid page faults on hot path
        for (size_t i = 0; i < Capacity; ++i) {
            new (&slots_[i]) T{};
        }
    }

    ~SPSCQueue() = default;

    // Non-copyable, non-movable
    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;
    SPSCQueue(SPSCQueue&&) = delete;
    SPSCQueue& operator=(SPSCQueue&&) = delete;

    // Producer: try to push an item
    // Returns true if successful, false if queue is full
    bool try_push(const T& item) {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t next_head = (head + 1) & (Capacity - 1);

        // Check if queue is full using cached tail
        if (next_head == cached_tail_) {
            // Cache miss - reload tail from consumer
            cached_tail_ = tail_.load(std::memory_order_acquire);
            if (next_head == cached_tail_) {
                return false;  // Queue is actually full
            }
        }

        // Write the item
        slots_[head] = item;

        // Publish to consumer
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    // Producer: push with spin-wait (use sparingly)
    void push(const T& item) {
        while (!try_push(item)) {
            // Spin - consider adding pause instruction or backoff
            #if defined(__x86_64__) || defined(_M_X64)
                __builtin_ia32_pause();
            #elif defined(__aarch64__)
                asm volatile("yield" ::: "memory");
            #endif
        }
    }

    // Consumer: try to pop an item
    // Returns std::nullopt if queue is empty
    std::optional<T> try_pop() {
        const size_t tail = tail_.load(std::memory_order_relaxed);

        // Check if queue is empty using cached head
        if (tail == cached_head_) {
            // Cache miss - reload head from producer
            cached_head_ = head_.load(std::memory_order_acquire);
            if (tail == cached_head_) {
                return std::nullopt;  // Queue is actually empty
            }
        }

        // Read the item
        T item = slots_[tail];

        // Advance tail
        tail_.store((tail + 1) & (Capacity - 1), std::memory_order_release);
        return item;
    }

    // Consumer: pop with spin-wait (use sparingly)
    T pop() {
        std::optional<T> item;
        while (!(item = try_pop())) {
            #if defined(__x86_64__) || defined(_M_X64)
                __builtin_ia32_pause();
            #elif defined(__aarch64__)
                asm volatile("yield" ::: "memory");
            #endif
        }
        return *item;
    }

    // Approximate size (racy but useful for monitoring)
    size_t size_approx() const {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t tail = tail_.load(std::memory_order_relaxed);
        return (head - tail + Capacity) & (Capacity - 1);
    }

    bool empty() const {
        return head_.load(std::memory_order_relaxed) == 
               tail_.load(std::memory_order_relaxed);
    }

    static constexpr size_t capacity() { return Capacity; }

private:
    // Producer side - head and cached_tail on same cache line
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_;
    size_t cached_tail_;

    // Consumer side - tail and cached_head on same cache line  
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_;
    size_t cached_head_;

    // Data slots on separate cache lines
    alignas(CACHE_LINE_SIZE) T slots_[Capacity];
};

// Multi-Producer Single-Consumer queue
// Not currently implemented - SPSC is sufficient for single order source
// Would be needed if multiple threads submit orders simultaneously
// Implementation would require atomic operations on head for producer contention

}  // namespace exchange
