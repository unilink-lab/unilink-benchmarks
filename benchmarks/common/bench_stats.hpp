#pragma once

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace unilink_bench {

struct LatencyStats {
  int64_t min_us = 0;
  double avg_us = 0.0;
  int64_t p50_us = 0;
  int64_t p95_us = 0;
  int64_t p99_us = 0;
  int64_t max_us = 0;
};

inline int64_t percentile(const std::vector<int64_t>& sorted_samples, double pct) {
  if (sorted_samples.empty()) {
    throw std::invalid_argument("percentile requires at least one sample");
  }

  const auto index = static_cast<size_t>((pct / 100.0) * static_cast<double>(sorted_samples.size() - 1));
  return sorted_samples[index];
}

inline LatencyStats compute_latency_stats(std::vector<int64_t> samples) {
  if (samples.empty()) {
    throw std::invalid_argument("compute_latency_stats requires at least one sample");
  }

  std::sort(samples.begin(), samples.end());

  const auto sum = std::accumulate(samples.begin(), samples.end(), int64_t{0});

  LatencyStats stats;
  stats.min_us = samples.front();
  stats.avg_us = static_cast<double>(sum) / static_cast<double>(samples.size());
  stats.p50_us = percentile(samples, 50.0);
  stats.p95_us = percentile(samples, 95.0);
  stats.p99_us = percentile(samples, 99.0);
  stats.max_us = samples.back();
  return stats;
}

}  // namespace unilink_bench
