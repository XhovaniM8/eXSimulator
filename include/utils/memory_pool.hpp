#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <vector>

namespace exchange {

// Fixed-size object pool with O(1) alloc/free
// Pre-allocates all memory upfront to avoid allocations on hot path
// Uses free-list for O(1) operations
template <typename T, size_t Capacity>
class MemoryPool {
    static_assert(sizeof(T) >= sizeof(void*), "T must be at least pointer-sized");
    
public:
    MemoryPool() {
        // Initialize free list
        for (size_t i = 0; i < Capacity - 1; ++i) {
            *reinterpret_cast<size_t*>(&storage_[i]) = i + 1;
        }
        *reinterpret_cast<size_t*>(&storage_[Capacity - 1]) = INVALID_INDEX;
        free_head_ = 0;
        allocated_ = 0;
    }
    
    ~MemoryPool() {
        // Note: Does not call destructors on allocated objects
        // User is responsible for freeing all objects before pool destruction
    }
    
    // Non-copyable, non-movable
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    MemoryPool(MemoryPool&&) = delete;
    MemoryPool& operator=(MemoryPool&&) = delete;
    
    // Allocate a slot and construct object in-place
    template <typename... Args>
    T* allocate(Args&&... args) {
        if (free_head_ == INVALID_INDEX) {
            return nullptr;  // Pool exhausted
        }
        
        size_t index = free_head_;
        free_head_ = *reinterpret_cast<size_t*>(&storage_[index]);
        ++allocated_;
        
        // Construct object in-place
        return new (&storage_[index]) T(std::forward<Args>(args)...);
    }
    
    // Deallocate slot (calls destructor)
    void deallocate(T* ptr) {
        if (!ptr) return;
        
        // Call destructor
        ptr->~T();
        
        // Add to free list
        size_t index = ptr - reinterpret_cast<T*>(&storage_[0]);
        *reinterpret_cast<size_t*>(&storage_[index]) = free_head_;
        free_head_ = index;
        --allocated_;
    }
    
    // Raw allocation without construction
    void* allocate_raw() {
        if (free_head_ == INVALID_INDEX) {
            return nullptr;
        }
        
        size_t index = free_head_;
        free_head_ = *reinterpret_cast<size_t*>(&storage_[index]);
        ++allocated_;
        
        return &storage_[index];
    }
    
    // Raw deallocation without destruction
    void deallocate_raw(void* ptr) {
        if (!ptr) return;
        
        size_t index = reinterpret_cast<Storage*>(ptr) - &storage_[0];
        *reinterpret_cast<size_t*>(&storage_[index]) = free_head_;
        free_head_ = index;
        --allocated_;
    }
    
    // Statistics
    size_t allocated() const { return allocated_; }
    size_t available() const { return Capacity - allocated_; }
    bool full() const { return free_head_ == INVALID_INDEX; }
    bool empty() const { return allocated_ == 0; }
    static constexpr size_t capacity() { return Capacity; }
    
    // Check if pointer belongs to this pool
    bool owns(const T* ptr) const {
        const auto* storage_ptr = reinterpret_cast<const Storage*>(ptr);
        return storage_ptr >= &storage_[0] && storage_ptr < &storage_[Capacity];
    }
    
private:
    static constexpr size_t INVALID_INDEX = ~size_t(0);
    
    // Storage with proper alignment
    struct alignas(alignof(T)) Storage {
        std::byte data[sizeof(T)];
    };
    
    std::array<Storage, Capacity> storage_;
    size_t free_head_;
    size_t allocated_;
};

// Slab allocator for variable-size allocations
// Maintains pools for different size classes
class SlabAllocator {
public:
    // Size classes: 64, 128, 256, 512, 1024, 2048, 4096 bytes
    static constexpr size_t NUM_SIZE_CLASSES = 7;
    static constexpr size_t MIN_SIZE = 64;
    static constexpr size_t MAX_SIZE = 4096;
    
    SlabAllocator() = default;
    
    void* allocate(size_t size) {
        if (size > MAX_SIZE) {
            // Fall back to malloc for large allocations
            return ::operator new(size);
        }
        
        size_t class_idx = size_class(size);
        // TODO: Implement size-class based allocation
        return ::operator new(size);
    }
    
    void deallocate(void* ptr, size_t size) {
        if (size > MAX_SIZE) {
            ::operator delete(ptr);
            return;
        }
        
        // TODO: Return to appropriate pool
        ::operator delete(ptr);
    }
    
private:
    static size_t size_class(size_t size) {
        if (size <= 64) return 0;
        if (size <= 128) return 1;
        if (size <= 256) return 2;
        if (size <= 512) return 3;
        if (size <= 1024) return 4;
        if (size <= 2048) return 5;
        return 6;
    }
};

}  // namespace exchange
