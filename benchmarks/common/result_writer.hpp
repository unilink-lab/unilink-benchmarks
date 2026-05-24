#pragma once

#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "common/bench_stats.hpp"

namespace unilink_bench {

struct LatencyResult {
  std::string_view transport;
  size_t payload_size = 0;
  size_t iterations = 0;
  size_t warmup_iterations = 0;
  double elapsed_seconds = 0.0;
  double messages_per_sec = 0.0;
  double mib_per_sec = 0.0;
  LatencyStats stats;
};

inline LatencyResult make_latency_result(std::string_view transport, size_t payload_size, size_t iterations,
                                         size_t warmup_iterations, double elapsed_seconds, const LatencyStats& stats) {
  const double messages_per_sec = static_cast<double>(iterations) / elapsed_seconds;
  const double mib_per_sec =
      (static_cast<double>(payload_size) * static_cast<double>(iterations)) / (1024.0 * 1024.0) / elapsed_seconds;

  return {transport, payload_size, iterations, warmup_iterations, elapsed_seconds, messages_per_sec, mib_per_sec, stats};
}

inline bool file_is_empty_or_missing(const std::string& path) {
  std::ifstream input(path);
  return !input.good() || input.peek() == std::ifstream::traits_type::eof();
}

inline void append_latency_csv(const std::string& path, const LatencyResult& result) {
  const bool write_header = file_is_empty_or_missing(path);
  std::ofstream output(path, std::ios::app);
  if (!output) {
    throw std::runtime_error("failed to open CSV output: " + path);
  }

  if (write_header) {
    output << "transport,payload_size,iterations,warmup_iterations,elapsed_s,messages_sec,mib_sec,min_us,avg_us,p50_us,"
              "p95_us,p99_us,p99_9_us,max_us";
    for (const auto& outlier : result.stats.outliers) {
      output << ",outliers_over_" << outlier.threshold_us << "us";
    }
    output << "\n";
  }

  output << result.transport << "," << result.payload_size << "," << result.iterations << ","
         << result.warmup_iterations << "," << std::fixed << std::setprecision(6) << result.elapsed_seconds << ","
         << result.messages_per_sec << "," << result.mib_per_sec << "," << result.stats.min_us << ","
         << result.stats.avg_us << "," << result.stats.p50_us << "," << result.stats.p95_us << ","
         << result.stats.p99_us << "," << result.stats.p99_9_us << "," << result.stats.max_us;
  for (const auto& outlier : result.stats.outliers) {
    output << "," << outlier.count;
  }
  output << "\n";
}

inline void print_result(const LatencyResult& result, const std::optional<std::string>& csv_output = std::nullopt) {
  std::cout << "transport: " << result.transport << "\n";
  std::cout << "payload_size: " << result.payload_size << " bytes\n";
  std::cout << "iterations: " << result.iterations << "\n";
  std::cout << "warmup_iterations: " << result.warmup_iterations << "\n";
  std::cout << "elapsed: " << std::fixed << std::setprecision(2) << result.elapsed_seconds << " s\n\n";
  std::cout << "throughput:\n";
  std::cout << "  messages/sec: " << std::fixed << std::setprecision(0) << result.messages_per_sec << "\n";
  std::cout << "  MiB/sec: " << std::fixed << std::setprecision(2) << result.mib_per_sec << "\n\n";
  std::cout << "latency_us:\n";
  std::cout << "  min: " << result.stats.min_us << "\n";
  std::cout << "  avg: " << std::fixed << std::setprecision(2) << result.stats.avg_us << "\n";
  std::cout << "  p50: " << result.stats.p50_us << "\n";
  std::cout << "  p95: " << result.stats.p95_us << "\n";
  std::cout << "  p99: " << result.stats.p99_us << "\n";
  std::cout << "  p99.9: " << result.stats.p99_9_us << "\n";
  std::cout << "  max: " << result.stats.max_us << "\n";
  for (const auto& outlier : result.stats.outliers) {
    std::cout << "  outliers_over_" << outlier.threshold_us << "us: " << outlier.count << "\n";
  }

  if (csv_output) {
    append_latency_csv(*csv_output, result);
    std::cout << "csv_output: " << *csv_output << "\n";
  }
}

}  // namespace unilink_bench
