#pragma once

#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string_view>

#include "common/bench_stats.hpp"

namespace unilink_bench {

inline void print_result(std::string_view transport, size_t payload_size, size_t iterations, double elapsed_seconds,
                         const LatencyStats& stats) {
  const double messages_per_sec = static_cast<double>(iterations) / elapsed_seconds;
  const double mib_per_sec =
      (static_cast<double>(payload_size) * static_cast<double>(iterations)) / (1024.0 * 1024.0) / elapsed_seconds;

  std::cout << "transport: " << transport << "\n";
  std::cout << "payload_size: " << payload_size << " bytes\n";
  std::cout << "iterations: " << iterations << "\n";
  std::cout << "elapsed: " << std::fixed << std::setprecision(2) << elapsed_seconds << " s\n\n";
  std::cout << "throughput:\n";
  std::cout << "  messages/sec: " << std::fixed << std::setprecision(0) << messages_per_sec << "\n";
  std::cout << "  MiB/sec: " << std::fixed << std::setprecision(2) << mib_per_sec << "\n\n";
  std::cout << "latency_us:\n";
  std::cout << "  min: " << stats.min_us << "\n";
  std::cout << "  avg: " << std::fixed << std::setprecision(2) << stats.avg_us << "\n";
  std::cout << "  p50: " << stats.p50_us << "\n";
  std::cout << "  p95: " << stats.p95_us << "\n";
  std::cout << "  p99: " << stats.p99_us << "\n";
  std::cout << "  max: " << stats.max_us << "\n";
}

}  // namespace unilink_bench
