#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <optional>
#include <type_traits>

namespace exchange {

constexpr size_t CACHE_LINE_SIZE = 64;

template <typename T, size_t Capacity>
class SPSCQueue {
    static_assert(Capacity > 0, "Capacity must be positive");
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

public:
    SPSCQueue() : head_(0), cached_tail_(0), tail_(0), cached_head_(0) {
        for (size_t i = 0; i < Capacity; ++i) {
            new (&slots_[i]) T{};
        }
    }

    ~SPSCQueue() = default;
    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;
    SPSCQueue(SPSCQueue&&) = delete;
    SPSCQueue& operator=(SPSCQueue&&) = delete;

    bool try_push(const T& item) {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t next_head = (head + 1) & (Capacity - 1);
        if (next_head == cached_tail_) {
            cached_tail_ = tail_.load(std::memory_order_acquire);
            if (next_head == cached_tail_) return false;
        }
        slots_[head] = item;
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    void push(const T& item) {
        while (!try_push(item)) {}
    }

    std::optional<T> try_pop() {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == cached_head_) {
            cached_head_ = head_.load(std::memory_order_acquire);
            if (tail == cached_head_) return std::nullopt;
        }
        T item = slots_[tail];
        tail_.store((tail + 1) & (Capacity - 1), std::memory_order_release);
        return item;
    }

    bool try_pop(T& item) {
        auto result = try_pop();
        if (result) { item = *result; return true; }
        return false;
    }

    T pop() {
        std::optional<T> item;
        while (!(item = try_pop())) {}
        return *item;
    }

    size_t size_approx() const {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t tail = tail_.load(std::memory_order_relaxed);
        return (head - tail + Capacity) & (Capacity - 1);
    }

    bool empty() const {
        return head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_relaxed);
    }

    static constexpr size_t capacity() { return Capacity; }

private:
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_;
    size_t cached_tail_;
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_;
    size_t cached_head_;
    alignas(CACHE_LINE_SIZE) T slots_[Capacity];
};

}  // namespace exchange
