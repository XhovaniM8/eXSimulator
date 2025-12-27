#include <catch2/catch_test_macros.hpp>
#include "utils/spsc_queue.hpp"

using namespace exchange;

TEST_CASE("SPSCQueue: Basic push and pop", "[spsc_queue]") {
    SPSCQueue<int, 16> queue;
    
    REQUIRE(queue.empty());
    REQUIRE(queue.size() == 0);
    
    bool success = queue.try_push(42);
    REQUIRE(success);
    REQUIRE_FALSE(queue.empty());
    REQUIRE(queue.size() == 1);
    
    int value;
    success = queue.try_pop(value);
    REQUIRE(success);
    REQUIRE(value == 42);
    REQUIRE(queue.empty());
}

TEST_CASE("SPSCQueue: Multiple elements", "[spsc_queue]") {
    SPSCQueue<int, 16> queue;
    
    // Push multiple elements
    for (int i = 0; i < 10; ++i) {
        REQUIRE(queue.try_push(i));
    }
    
    REQUIRE(queue.size() == 10);
    
    // Pop and verify order
    for (int i = 0; i < 10; ++i) {
        int value;
        REQUIRE(queue.try_pop(value));
        REQUIRE(value == i);
    }
    
    REQUIRE(queue.empty());
}

TEST_CASE("SPSCQueue: Full queue", "[spsc_queue]") {
    SPSCQueue<int, 16> queue;
    
    // Fill queue to capacity - 1 (one slot reserved)
    for (int i = 0; i < 15; ++i) {
        REQUIRE(queue.try_push(i));
    }
    
    // Next push should fail (queue full)
    REQUIRE_FALSE(queue.try_push(999));
    
    // Pop one element
    int value;
    REQUIRE(queue.try_pop(value));
    REQUIRE(value == 0);
    
    // Now we can push again
    REQUIRE(queue.try_push(999));
}

TEST_CASE("SPSCQueue: Pop from empty queue", "[spsc_queue]") {
    SPSCQueue<int, 16> queue;
    
    int value;
    bool success = queue.try_pop(value);
    REQUIRE_FALSE(success);
}

TEST_CASE("SPSCQueue: Front peek", "[spsc_queue]") {
    SPSCQueue<int, 16> queue;
    
    // Empty queue
    REQUIRE(queue.front() == nullptr);
    
    // Push element
    queue.try_push(42);
    
    // Peek without removing
    const int* front = queue.front();
    REQUIRE(front != nullptr);
    REQUIRE(*front == 42);
    
    // Verify element still in queue
    REQUIRE_FALSE(queue.empty());
    
    // Pop and verify
    int value;
    queue.try_pop(value);
    REQUIRE(value == 42);
}

TEST_CASE("SPSCQueue: Capacity", "[spsc_queue]") {
    SPSCQueue<int, 32> queue;
    REQUIRE(queue.capacity() == 32);
}

TEST_CASE("SPSCQueue: Wrap around", "[spsc_queue]") {
    SPSCQueue<int, 8> queue;
    
    // Fill and empty multiple times to test wrap around
    for (int cycle = 0; cycle < 3; ++cycle) {
        // Fill queue
        for (int i = 0; i < 7; ++i) {
            REQUIRE(queue.try_push(i + cycle * 10));
        }
        
        // Empty queue
        for (int i = 0; i < 7; ++i) {
            int value;
            REQUIRE(queue.try_pop(value));
            REQUIRE(value == i + cycle * 10);
        }
        
        REQUIRE(queue.empty());
    }
}

TEST_CASE("SPSCQueue: Struct element", "[spsc_queue]") {
    struct TestStruct {
        int a;
        double b;
        char c;
    };
    
    SPSCQueue<TestStruct, 16> queue;
    
    TestStruct data{42, 3.14, 'x'};
    REQUIRE(queue.try_push(data));
    
    TestStruct result;
    REQUIRE(queue.try_pop(result));
    REQUIRE(result.a == 42);
    REQUIRE(result.b == 3.14);
    REQUIRE(result.c == 'x');
}

TEST_CASE("SPSCQueue: Interleaved push and pop", "[spsc_queue]") {
    SPSCQueue<int, 16> queue;
    
    // Interleave pushes and pops
    for (int i = 0; i < 20; ++i) {
        REQUIRE(queue.try_push(i));
        
        if (i % 2 == 0) {
            int value;
            REQUIRE(queue.try_pop(value));
        }
    }
    
    // Should have 10 elements remaining
    REQUIRE(queue.size() == 10);
    
    // Pop remaining elements
    for (int i = 0; i < 10; ++i) {
        int value;
        REQUIRE(queue.try_pop(value));
    }
    
    REQUIRE(queue.empty());
}
