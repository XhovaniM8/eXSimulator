cmake_minimum_required(VERSION 3.20)
project(exchange_simulator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type defaults
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -march=native -g -fno-omit-frame-pointer")

# Common warning flags
add_compile_options(
    -Wall -Wextra -Wpedantic -Werror
    -Wno-unused-parameter
    -Wconversion -Wsign-conversion
    -Wshadow -Wdouble-promotion
      # HFT style: no exceptions on hot path
    -fno-rtti        # HFT style: no RTTI overhead
)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Core library
add_library(exchange_core STATIC
  src/core/types.cpp
  src/core/order.cpp
)


# Engine library
add_library(exchange_engine STATIC
    src/engine/order_book.cpp
    src/engine/matching_engine.cpp
    src/engine/price_level.cpp
)
target_link_libraries(exchange_engine PUBLIC exchange_core)

# Utils library
add_library(exchange_utils STATIC
    src/utils/spsc_queue.cpp
    src/utils/histogram.cpp
    src/utils/timing.cpp
)

# Agents library
add_library(exchange_agents STATIC
    src/agents/market_maker.cpp
    src/agents/momentum.cpp
    src/agents/noise.cpp
)
target_link_libraries(exchange_agents PUBLIC exchange_core)

# Replay library
add_library(exchange_replay STATIC
    src/replay/event_journal.cpp
    src/replay/replay_harness.cpp
)
target_link_libraries(exchange_replay PUBLIC exchange_core exchange_utils)

# Network library (optional, requires boost/asio)
option(BUILD_NETWORK "Build network components" OFF)
if(BUILD_NETWORK)
    find_package(Boost REQUIRED COMPONENTS system)
    add_library(exchange_network STATIC
        src/network/gateway.cpp
        src/network/websocket_publisher.cpp
    )
    target_link_libraries(exchange_network PUBLIC 
        exchange_core 
        exchange_utils
        Boost::system
    )
endif()

# Main executable
add_executable(exchange_sim src/main.cpp)
target_link_libraries(exchange_sim PRIVATE
    exchange_engine
    exchange_agents
    exchange_replay
    exchange_utils
)

# Feature test executable
add_executable(feature_test src/feature_test.cpp)
target_link_libraries(feature_test PRIVATE
    exchange_engine
    exchange_utils
)

# Benchmarks
option(BUILD_BENCHMARKS "Build benchmark suite" ON)
if(BUILD_BENCHMARKS)
    find_package(benchmark QUIET)
    if(benchmark_FOUND)
        add_executable(benchmarks
            tests/benchmarks/order_book_bench.cpp
            tests/benchmarks/matching_bench.cpp
            tests/benchmarks/queue_bench.cpp
        )
        target_link_libraries(benchmarks PRIVATE
            exchange_engine
            exchange_utils
            benchmark::benchmark
        )
        # Relax strict flags for benchmark code (Google Benchmark requires RTTI)
        target_compile_options(benchmarks PRIVATE -frtti -Wno-sign-conversion -Wno-conversion -Wno-deprecated-declarations)
    else()
        message(STATUS "Google Benchmark not found, skipping benchmark build")
    endif()
endif()

# Tests
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    find_package(Catch2 3 QUIET)
    if(Catch2_FOUND)
        add_executable(unit_tests
            tests/unit/order_test.cpp
        )
        target_link_libraries(unit_tests PRIVATE
            exchange_engine
            exchange_utils
            Catch2::Catch2WithMain
        )
        include(CTest)
        include(Catch)
        catch_discover_tests(unit_tests)
    else()
        message(STATUS "Catch2 not found, skipping test build")
    endif()
endif()

# Install
install(TARGETS exchange_sim RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)

# Unit tests
add_executable(test_order_book
    tests/test_order_book.cpp
)
target_link_libraries(test_order_book PRIVATE 
    exchange_engine 
    exchange_core
    exchange_utils
)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Kraken replay benchmark
add_executable(replay_kraken src/tools/replay_kraken.cpp)
target_link_libraries(replay_kraken PRIVATE
    exchange_engine
    exchange_core
    exchange_utils
)
target_compile_options(replay_kraken PRIVATE -Wno-conversion -Wno-sign-conversion -Wno-error)
