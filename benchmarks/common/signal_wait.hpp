#pragma once

#include <chrono>
#include <csignal>
#include <thread>

namespace unilink_bench {

inline volatile std::sig_atomic_t& stop_requested_flag() {
  static volatile std::sig_atomic_t stop_requested = 0;
  return stop_requested;
}

inline void request_stop(int) { stop_requested_flag() = 1; }

inline void install_signal_handlers() {
  std::signal(SIGINT, request_stop);
  std::signal(SIGTERM, request_stop);
}

inline void wait_for_stop_signal() {
  while (!stop_requested_flag()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

}  // namespace unilink_bench
