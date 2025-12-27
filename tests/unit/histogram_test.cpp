#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "utils/histogram.hpp"

using namespace exchange;
using Catch::Matchers::WithinRel;

TEST_CASE("LatencyHistogram: Basic recording", "[histogram]") {
    LatencyHistogram hist;
    
    REQUIRE(hist.count() == 0);
    REQUIRE(hist.sum() == 0);
    REQUIRE(hist.min() == 0);
    REQUIRE(hist.max() == 0);
    
    hist.record(100);
    
    REQUIRE(hist.count() == 1);
    REQUIRE(hist.sum() == 100);
    REQUIRE(hist.min() == 100);
    REQUIRE(hist.max() == 100);
}

TEST_CASE("LatencyHistogram: Multiple values", "[histogram]") {
    LatencyHistogram hist;
    
    hist.record(100);
    hist.record(200);
    hist.record(300);
    
    REQUIRE(hist.count() == 3);
    REQUIRE(hist.sum() == 600);
    REQUIRE(hist.min() == 100);
    REQUIRE(hist.max() == 300);
    REQUIRE_THAT(hist.mean(), WithinRel(200.0, 0.01));
}

TEST_CASE("LatencyHistogram: Percentiles", "[histogram]") {
    LatencyHistogram hist;
    
    // Record 100 values from 1 to 100
    for (int i = 1; i <= 100; ++i) {
        hist.record(i);
    }
    
    // Check percentiles (approximate due to bucketing)
    REQUIRE(hist.p50() > 40);
    REQUIRE(hist.p50() < 60);
    REQUIRE(hist.p90() > 80);
    REQUIRE(hist.p99() > 95);
}

TEST_CASE("LatencyHistogram: Reset", "[histogram]") {
    LatencyHistogram hist;
    
    hist.record(100);
    hist.record(200);
    
    REQUIRE(hist.count() == 2);
    
    hist.reset();
    
    REQUIRE(hist.count() == 0);
    REQUIRE(hist.sum() == 0);
    REQUIRE(hist.min() == 0);
    REQUIRE(hist.max() == 0);
}

TEST_CASE("LatencyHistogram: Merge histograms", "[histogram]") {
    LatencyHistogram hist1;
    LatencyHistogram hist2;
    
    hist1.record(100);
    hist1.record(200);
    
    hist2.record(300);
    hist2.record(400);
    
    hist1.merge(hist2);
    
    REQUIRE(hist1.count() == 4);
    REQUIRE(hist1.sum() == 1000);
    REQUIRE(hist1.min() == 100);
    REQUIRE(hist1.max() == 400);
}

TEST_CASE("LatencyHistogram: Extreme values", "[histogram]") {
    LatencyHistogram hist;
    
    hist.record(1);  // Minimum
    hist.record(1000000000);  // Maximum (1 second)
    hist.record(50000);  // Middle value
    
    REQUIRE(hist.count() == 3);
    REQUIRE(hist.min() == 1);
    REQUIRE(hist.max() == 1000000000);
}

TEST_CASE("LatencyHistogram: Common percentiles", "[histogram]") {
    LatencyHistogram hist;
    
    // Record latencies in microseconds (1000-10000 ns)
    for (int i = 1000; i <= 10000; i += 100) {
        hist.record(i);
    }
    
    // Verify percentile methods exist and return reasonable values
    REQUIRE(hist.p50() >= 1000);
    REQUIRE(hist.p90() >= 1000);
    REQUIRE(hist.p95() >= 1000);
    REQUIRE(hist.p99() >= 1000);
    REQUIRE(hist.p999() >= 1000);
}

TEST_CASE("LatencyHistogram: Bucket boundaries", "[histogram]") {
    auto bounds = LatencyHistogram::bucket_boundaries();
    
    REQUIRE(bounds.size() == LatencyHistogram::NUM_BUCKETS + 1);
    REQUIRE(bounds[0] == 0);
    REQUIRE(bounds[bounds.size() - 1] == LatencyHistogram::MAX_VALUE);
}

TEST_CASE("CounterHistogram: Basic recording", "[histogram]") {
    CounterHistogram<64, 1000> hist;
    
    REQUIRE(hist.count() == 0);
    REQUIRE(hist.sum() == 0);
    
    hist.record(100);
    
    REQUIRE(hist.count() == 1);
    REQUIRE(hist.sum() == 100);
}

TEST_CASE("CounterHistogram: Multiple values", "[histogram]") {
    CounterHistogram<64, 1000> hist;
    
    hist.record(100);
    hist.record(200);
    hist.record(300);
    
    REQUIRE(hist.count() == 3);
    REQUIRE(hist.sum() == 600);
    REQUIRE_THAT(hist.mean(), WithinRel(200.0, 0.01));
}

TEST_CASE("CounterHistogram: Reset", "[histogram]") {
    CounterHistogram<64, 1000> hist;
    
    hist.record(100);
    hist.record(200);
    
    REQUIRE(hist.count() == 2);
    
    hist.reset();
    
    REQUIRE(hist.count() == 0);
    REQUIRE(hist.sum() == 0);
}
