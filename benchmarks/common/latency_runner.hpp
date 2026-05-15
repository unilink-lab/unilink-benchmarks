#pragma once

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "common/bench_stats.hpp"
#include "common/bench_timer.hpp"
#include "common/payload.hpp"
#include "common/result_writer.hpp"

namespace unilink_bench {

class EchoWaiter {
 public:
  void on_bytes(std::string_view bytes) {
    auto frames = decoder_.push(bytes);
    if (frames.empty()) {
      return;
    }

    {
      std::lock_guard<std::mutex> lock(mutex_);
      received_payload_ = std::move(frames.back());
      received_ = true;
    }
    cv_.notify_one();
  }

  void on_error(std::string_view message) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      error_ = std::string(message);
    }
    cv_.notify_one();
  }

  void reset_iteration() {
    std::lock_guard<std::mutex> lock(mutex_);
    received_ = false;
    received_payload_.clear();
  }

  std::string wait_for_echo(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    const bool ready = cv_.wait_for(lock, timeout, [this] { return received_ || !error_.empty(); });

    if (!error_.empty()) {
      throw std::runtime_error(error_);
    }
    if (!ready) {
      throw std::runtime_error("timed out waiting for echo");
    }

    return received_payload_;
  }

 private:
  FrameDecoder decoder_;
  std::mutex mutex_;
  std::condition_variable cv_;
  bool received_ = false;
  std::string received_payload_;
  std::string error_;
};

template <typename Client>
int run_latency_client(std::string_view transport, Client& client, size_t payload_size, size_t iterations) {
  if (!client.start_sync()) {
    std::cerr << "Failed to start " << transport << " client\n";
    return 1;
  }

  const std::string payload = make_payload(payload_size);
  const std::string frame = make_frame(payload);
  std::vector<int64_t> samples;
  samples.reserve(iterations);

  const auto total_start = now();
  for (size_t i = 0; i < iterations; ++i) {
    auto& waiter = client.echo_waiter();
    waiter.reset_iteration();

    const auto start = now();
    if (!client.send_frame(frame)) {
      std::cerr << "Failed to send payload at iteration " << i << "\n";
      client.stop();
      return 1;
    }

    const std::string echoed = waiter.wait_for_echo(std::chrono::seconds(5));
    const auto end = now();

    if (!payload_matches(payload, echoed)) {
      std::cerr << "Echo payload mismatch at iteration " << i << "\n";
      client.stop();
      return 1;
    }

    samples.push_back(elapsed_us(start, end));
  }
  const auto total_end = now();

  client.stop();

  print_result(transport, payload_size, iterations, seconds_between(total_start, total_end),
               compute_latency_stats(std::move(samples)));
  return 0;
}

}  // namespace unilink_bench
