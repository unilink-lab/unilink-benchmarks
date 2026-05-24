#pragma once

#include <algorithm>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>

namespace unilink_bench {

struct OutlierCount {
  int64_t threshold_us = 0;
  size_t count = 0;
};

struct LatencyStats {
  int64_t min_us = 0;
  double avg_us = 0.0;
  int64_t p50_us = 0;
  int64_t p95_us = 0;
  int64_t p99_us = 0;
  int64_t p99_9_us = 0;
  int64_t max_us = 0;
  std::vector<OutlierCount> outliers;
};

inline int64_t percentile(const std::vector<int64_t>& sorted_samples, double pct) {
  if (sorted_samples.empty()) {
    throw std::invalid_argument("percentile requires at least one sample");
  }

  const auto index = static_cast<size_t>((pct / 100.0) * static_cast<double>(sorted_samples.size() - 1));
  return sorted_samples[index];
}

inline std::vector<int64_t> default_outlier_thresholds_us() {
  return {5000, 10000, 50000};
}

inline std::vector<int64_t> parse_outlier_thresholds_us(const char* value) {
  if (value == nullptr || std::string(value).empty()) {
    return default_outlier_thresholds_us();
  }

  std::istringstream input(value);
  std::vector<int64_t> thresholds;
  std::string token;
  while (input >> token) {
    const auto parsed = std::stoll(token);
    if (parsed <= 0) {
      throw std::invalid_argument("OUTLIER_THRESHOLDS_US values must be greater than zero");
    }
    thresholds.push_back(parsed);
  }

  if (thresholds.empty()) {
    return default_outlier_thresholds_us();
  }

  std::sort(thresholds.begin(), thresholds.end());
  thresholds.erase(std::unique(thresholds.begin(), thresholds.end()), thresholds.end());
  return thresholds;
}

inline std::vector<int64_t> outlier_thresholds_us_from_env() {
  return parse_outlier_thresholds_us(std::getenv("OUTLIER_THRESHOLDS_US"));
}

inline size_t count_outliers_over(const std::vector<int64_t>& sorted_samples, int64_t threshold_us) {
  return static_cast<size_t>(sorted_samples.end() - std::upper_bound(sorted_samples.begin(), sorted_samples.end(),
                                                                      threshold_us));
}

inline LatencyStats compute_latency_stats(std::vector<int64_t> samples,
                                          std::vector<int64_t> outlier_thresholds_us =
                                              outlier_thresholds_us_from_env()) {
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
  stats.p99_9_us = percentile(samples, 99.9);
  stats.max_us = samples.back();
  for (const auto threshold : outlier_thresholds_us) {
    stats.outliers.push_back({threshold, count_outliers_over(samples, threshold)});
  }
  return stats;
}

}  // namespace unilink_bench
