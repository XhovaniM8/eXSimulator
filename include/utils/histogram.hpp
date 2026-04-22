#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace exchange {

// High-performance latency histogram using log-linear bucketing
// Inspired by HdrHistogram but simpler and header-only
// Tracks nanosecond latencies from 1ns to ~1 second
class LatencyHistogram {
public:
  static constexpr size_t NUM_BUCKETS = 256;
  static constexpr uint64_t MIN_VALUE = 1;          // 1 ns
  static constexpr uint64_t MAX_VALUE = 1000000000; // 1 second

  LatencyHistogram() { reset(); }

  void reset() {
    count_ = 0;
    sum_ = 0;
    min_ = MAX_VALUE;
    max_ = 0;
    buckets_.fill(0);
  }

  // Record a latency value in nanoseconds
  void record(uint64_t value) {
    ++count_;
    sum_ += value;
    min_ = std::min(min_, value);
    max_ = std::max(max_, value);

    size_t bucket = value_to_bucket(value);
    ++buckets_[bucket];
  }

  // Get percentile value (0-100)
  uint64_t percentile(double p) const {
    if (count_ == 0)
      return 0;

    uint64_t target =
        static_cast<uint64_t>(static_cast<double>(count_) * p / 100.0);
    uint64_t cumulative = 0;

    for (size_t i = 0; i < NUM_BUCKETS; ++i) {
      cumulative += buckets_[i];
      if (cumulative >= target) {
        return bucket_to_value(i);
      }
    }
    return max_;
  }

  // Common percentiles
  uint64_t p50() const { return percentile(50); }
  uint64_t p90() const { return percentile(90); }
  uint64_t p95() const { return percentile(95); }
  uint64_t p99() const { return percentile(99); }
  uint64_t p999() const { return percentile(99.9); }

  // Statistics
  uint64_t count() const { return count_; }
  uint64_t sum() const { return sum_; }
  uint64_t min() const { return count_ > 0 ? min_ : 0; }
  uint64_t max() const { return max_; }
  double mean() const {
    return count_ > 0 ? static_cast<double>(sum_) / static_cast<double>(count_)
                      : 0;
  }

  // Merge another histogram into this one
  void merge(const LatencyHistogram &other) {
    count_ += other.count_;
    sum_ += other.sum_;
    min_ = std::min(min_, other.min_);
    max_ = std::max(max_, other.max_);
    for (size_t i = 0; i < NUM_BUCKETS; ++i) {
      buckets_[i] += other.buckets_[i];
    }
  }

  // Get bucket counts for visualization
  const std::array<uint64_t, NUM_BUCKETS> &buckets() const { return buckets_; }

  // Get bucket boundaries for visualization
  static std::vector<uint64_t> bucket_boundaries() {
    std::vector<uint64_t> bounds;
    bounds.reserve(NUM_BUCKETS + 1);
    for (size_t i = 0; i <= NUM_BUCKETS; ++i) {
      bounds.push_back(bucket_to_value(i));
    }
    return bounds;
  }

private:
  // Log-linear bucketing: first 128 buckets are linear (1ns each)
  // Remaining buckets are logarithmic
  static size_t value_to_bucket(uint64_t value) {
    if (value < 128)
      return value;
    if (value >= MAX_VALUE)
      return NUM_BUCKETS - 1;

    // Log2 of value, mapped to remaining buckets
    unsigned int log = static_cast<unsigned int>(63 - __builtin_clzll(value));
    size_t bucket = 128 + (log - 7) * 16 + ((value >> (log - 4)) & 0xF);
    return std::min(bucket, NUM_BUCKETS - 1);
  }

  static uint64_t bucket_to_value(size_t bucket) {
    if (bucket < 128)
      return bucket;
    if (bucket >= NUM_BUCKETS)
      return MAX_VALUE;

    size_t log = 7 + (bucket - 128) / 16;
    size_t frac = (bucket - 128) % 16;
    return (1ULL << log) + (frac << (log - 4));
  }

  uint64_t count_;
  uint64_t sum_;
  uint64_t min_;
  uint64_t max_;
  std::array<uint64_t, NUM_BUCKETS> buckets_;
};

// Simple counter histogram (for order sizes, etc.)
template <size_t NumBuckets = 64, uint64_t MaxValue = 1000000>
class CounterHistogram {
public:
  CounterHistogram() { reset(); }

  void reset() {
    count_ = 0;
    sum_ = 0;
    buckets_.fill(0);
  }

  void record(uint64_t value) {
    ++count_;
    sum_ += value;
    size_t bucket =
        std::min<size_t>(value * NumBuckets / MaxValue, NumBuckets - 1);
    ++buckets_[bucket];
  }

  uint64_t count() const { return count_; }
  uint64_t sum() const { return sum_; }
  double mean() const {
    return count_ > 0 ? static_cast<double>(sum_) / count_ : 0;
  }

private:
  uint64_t count_;
  uint64_t sum_;
  std::array<uint64_t, NumBuckets> buckets_;
};

} // namespace exchange
