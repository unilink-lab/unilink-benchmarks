#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "common/bench_config.hpp"
#include "unilink/unilink.hpp"

namespace {

using Strategy = unilink::base::constants::BackpressureStrategy;
using Clock = std::chrono::steady_clock;
constexpr uint64_t kMaxConsecutiveTimeouts = 10;

struct SmokeConfig {
  std::string host = unilink_bench::kDefaultHost;
  uint16_t port = 9401;
  std::vector<size_t> payload_sizes{1024, 4096, 16384};
  size_t iterations = 1000;
  size_t warmup_iterations = 100;
  int timeout_ms = 1000;
  std::string strategy = "both";
  std::string csv_output = "build/udp_payload_smoke.csv";
};

struct SmokeResult {
  std::string strategy;
  size_t payload_size = 0;
  size_t iterations = 0;
  size_t warmup_iterations = 0;
  uint64_t send_attempts = 0;
  uint64_t send_success = 0;
  uint64_t send_failures = 0;
  uint64_t server_received = 0;
  uint64_t client_received = 0;
  uint64_t echo_matches = 0;
  uint64_t timeouts = 0;
  bool stopped_early = false;
  std::string stop_reason = "none";
  uint64_t consecutive_timeout_limit = kMaxConsecutiveTimeouts;
  double delivery_percent = 0.0;
  double match_percent = 0.0;
  double elapsed_ms = 0.0;
  double messages_per_sec = 0.0;
  bool runtime_stats_supported = false;
  uint64_t client_failed_sends = 0;
  uint64_t client_dropped_messages = 0;
  uint64_t client_dropped_bytes = 0;
  uint64_t client_backpressure_events = 0;
  size_t client_max_queued_bytes = 0;
  size_t client_queued_bytes_final = 0;
  uint64_t server_messages_received = 0;
  uint64_t server_bytes_received = 0;
  uint64_t server_failed_sends = 0;
  uint64_t server_dropped_messages = 0;
  uint64_t server_dropped_bytes = 0;
  uint64_t server_backpressure_events = 0;
  size_t server_max_queued_bytes = 0;
  size_t server_queued_bytes_final = 0;
  std::string notes = "ok";
};

struct RuntimeStatsSnapshot {
  uint64_t failed_sends = 0;
  uint64_t dropped_messages = 0;
  uint64_t dropped_bytes = 0;
  uint64_t backpressure_events = 0;
  size_t max_queued_bytes = 0;
  size_t queued_bytes = 0;
  uint64_t messages_received = 0;
  uint64_t bytes_received = 0;
};

struct EchoState {
  std::mutex mutex;
  std::condition_variable cv;
  std::string expected;
  bool received = false;
};

struct SendOutcome {
  bool accepted = false;
  bool echoed = false;
};

std::vector<size_t> parse_payload_sizes(const std::string& value) {
  std::istringstream input(value);
  std::vector<size_t> sizes;
  std::string token;
  while (input >> token) {
    sizes.push_back(unilink_bench::parse_size(token, "payload size"));
  }
  if (sizes.empty()) {
    throw std::invalid_argument("payload sizes must not be empty");
  }
  return sizes;
}

void print_usage(const char* program) {
  std::cerr
      << "Usage: " << program
      << " [--host HOST] [--port PORT] [--payload-sizes \"1024 4096 16384\"]\n"
      << "       [--iterations COUNT] [--warmup-iterations COUNT] "
         "[--timeout-ms MS]\n"
      << "       [--strategy reliable|besteffort|both] [--csv-output PATH]\n";
}

SmokeConfig parse_args(int argc, char** argv) {
  SmokeConfig config;
  for (int i = 1; i < argc; ++i) {
    std::string value;
    const std::string arg = argv[i];
    if (arg == "--host" && unilink_bench::read_option(argc, argv, i, &value)) {
      config.host = value;
    } else if (arg == "--port" &&
               unilink_bench::read_option(argc, argv, i, &value)) {
      config.port = unilink_bench::parse_port(value);
    } else if (arg == "--payload-sizes" &&
               unilink_bench::read_option(argc, argv, i, &value)) {
      config.payload_sizes = parse_payload_sizes(value);
    } else if (arg == "--iterations" &&
               unilink_bench::read_option(argc, argv, i, &value)) {
      config.iterations = unilink_bench::parse_size(value, "iterations");
    } else if (arg == "--warmup-iterations" &&
               unilink_bench::read_option(argc, argv, i, &value)) {
      config.warmup_iterations =
          unilink_bench::parse_nonnegative_size(value, "warmup iterations");
    } else if (arg == "--timeout-ms" &&
               unilink_bench::read_option(argc, argv, i, &value)) {
      config.timeout_ms = unilink_bench::parse_positive_int(value, "timeout");
    } else if (arg == "--strategy" &&
               unilink_bench::read_option(argc, argv, i, &value)) {
      if (value != "reliable" && value != "besteffort" && value != "both") {
        throw std::invalid_argument(
            "strategy must be reliable, besteffort, or both");
      }
      config.strategy = value;
    } else if (arg == "--csv-output" &&
               unilink_bench::read_option(argc, argv, i, &value)) {
      config.csv_output = value;
    } else {
      print_usage(argv[0]);
      throw std::invalid_argument("unknown or incomplete option: " + arg);
    }
  }
  return config;
}

std::vector<Strategy> strategies_for(const std::string& value) {
  if (value == "reliable") {
    return {Strategy::Reliable};
  }
  if (value == "besteffort") {
    return {Strategy::BestEffort};
  }
  return {Strategy::Reliable, Strategy::BestEffort};
}

std::string strategy_name(Strategy strategy) {
  return strategy == Strategy::Reliable ? "reliable" : "besteffort";
}

std::string make_payload(size_t size, uint64_t sequence) {
  std::string payload(size, '\0');
  for (size_t i = 0; i < size; ++i) {
    payload[i] = static_cast<char>((i * 131 + sequence * 17) & 0xFF);
  }
  const auto prefix = std::min<size_t>(sizeof(sequence), payload.size());
  for (size_t i = 0; i < prefix; ++i) {
    payload[i] = static_cast<char>((sequence >> (i * 8)) & 0xFF);
  }
  return payload;
}

bool file_is_empty_or_missing(const std::string& path) {
  std::ifstream input(path);
  return !input.good() || input.peek() == std::ifstream::traits_type::eof();
}

std::string notes_for(const SmokeResult& result) {
  std::vector<std::string> notes;
  if (result.send_failures > 0) {
    notes.emplace_back("send-failures");
  }
  if (result.timeouts > 0) {
    notes.emplace_back("timeouts");
  }
  if (result.stopped_early) {
    notes.emplace_back("early-stop");
  }
  if (result.client_received > result.echo_matches) {
    notes.emplace_back("mismatches");
  }
  if (notes.empty()) {
    return "ok";
  }
  std::string joined;
  for (const auto& note : notes) {
    if (!joined.empty()) {
      joined += ";";
    }
    joined += note;
  }
  return joined;
}

#ifdef UNILINK_BENCH_HAS_RUNTIME_STATS
template <typename Endpoint>
RuntimeStatsSnapshot read_runtime_stats(const Endpoint& endpoint) {
  const auto stats = endpoint.stats();
  RuntimeStatsSnapshot snapshot;
  snapshot.failed_sends = stats.failed_sends;
  snapshot.dropped_messages = stats.dropped_messages;
  snapshot.dropped_bytes = stats.dropped_bytes;
  snapshot.backpressure_events = stats.backpressure_events;
  snapshot.max_queued_bytes = stats.max_queued_bytes;
  snapshot.queued_bytes = stats.queued_bytes;
  snapshot.messages_received = stats.messages_received;
  snapshot.bytes_received = stats.bytes_received;
  return snapshot;
}

void record_runtime_stats(SmokeResult& result,
                          const RuntimeStatsSnapshot& client_before,
                          const RuntimeStatsSnapshot& client_after,
                          const RuntimeStatsSnapshot& server_before,
                          const RuntimeStatsSnapshot& server_after) {
  result.runtime_stats_supported = true;
  result.client_failed_sends =
      client_after.failed_sends - client_before.failed_sends;
  result.client_dropped_messages =
      client_after.dropped_messages - client_before.dropped_messages;
  result.client_dropped_bytes =
      client_after.dropped_bytes - client_before.dropped_bytes;
  result.client_backpressure_events =
      client_after.backpressure_events - client_before.backpressure_events;
  result.client_max_queued_bytes = client_after.max_queued_bytes;
  result.client_queued_bytes_final = client_after.queued_bytes;
  result.server_messages_received =
      server_after.messages_received - server_before.messages_received;
  result.server_bytes_received =
      server_after.bytes_received - server_before.bytes_received;
  result.server_failed_sends =
      server_after.failed_sends - server_before.failed_sends;
  result.server_dropped_messages =
      server_after.dropped_messages - server_before.dropped_messages;
  result.server_dropped_bytes =
      server_after.dropped_bytes - server_before.dropped_bytes;
  result.server_backpressure_events =
      server_after.backpressure_events - server_before.backpressure_events;
  result.server_max_queued_bytes = server_after.max_queued_bytes;
  result.server_queued_bytes_final = server_after.queued_bytes;
}
#else
template <typename Endpoint>
RuntimeStatsSnapshot read_runtime_stats(const Endpoint&) {
  return {};
}

void record_runtime_stats(SmokeResult&, const RuntimeStatsSnapshot&,
                          const RuntimeStatsSnapshot&,
                          const RuntimeStatsSnapshot&,
                          const RuntimeStatsSnapshot&) {}
#endif

void append_csv(const std::string& path, const SmokeResult& result) {
  std::filesystem::path output_path(path);
  if (output_path.has_parent_path()) {
    std::filesystem::create_directories(output_path.parent_path());
  }

  const bool write_header = file_is_empty_or_missing(path);
  std::ofstream output(path, std::ios::app);
  if (!output) {
    throw std::runtime_error("failed to open CSV output: " + path);
  }

  if (write_header) {
    output << "transport,strategy,payload_size,iterations,warmup_iterations,"
              "send_attempts,send_success,"
              "send_failures,server_received,client_received,echo_matches,"
              "timeouts,stopped_early,stop_reason,"
              "consecutive_timeout_limit,delivery_percent,match_percent,"
              "elapsed_ms,messages_per_sec,"
              "runtime_stats_supported,client_failed_sends,client_dropped_"
              "messages,client_dropped_bytes,"
              "client_backpressure_events,client_max_queued_bytes,client_"
              "queued_bytes_final,"
              "server_messages_received,server_bytes_received,server_failed_"
              "sends,server_dropped_messages,"
              "server_dropped_bytes,server_backpressure_events,server_max_"
              "queued_bytes,"
              "server_queued_bytes_final,notes\n";
  }

  output << "udp," << result.strategy << "," << result.payload_size << ","
         << result.iterations << "," << result.warmup_iterations << ","
         << result.send_attempts << "," << result.send_success << ","
         << result.send_failures << "," << result.server_received << ","
         << result.client_received << "," << result.echo_matches << ","
         << result.timeouts << "," << (result.stopped_early ? "true" : "false")
         << "," << result.stop_reason << "," << result.consecutive_timeout_limit
         << "," << std::fixed << std::setprecision(6) << result.delivery_percent
         << "," << result.match_percent << "," << result.elapsed_ms << ","
         << result.messages_per_sec << ","
         << (result.runtime_stats_supported ? "true" : "false") << ","
         << result.client_failed_sends << "," << result.client_dropped_messages
         << "," << result.client_dropped_bytes << ","
         << result.client_backpressure_events << ","
         << result.client_max_queued_bytes << ","
         << result.client_queued_bytes_final << ","
         << result.server_messages_received << ","
         << result.server_bytes_received << "," << result.server_failed_sends
         << "," << result.server_dropped_messages << ","
         << result.server_dropped_bytes << ","
         << result.server_backpressure_events << ","
         << result.server_max_queued_bytes << ","
         << result.server_queued_bytes_final << "," << result.notes << "\n";
}

SendOutcome send_and_wait(unilink::UdpClient& client, EchoState& state,
                          const std::string& payload,
                          std::chrono::milliseconds timeout) {
  {
    std::lock_guard<std::mutex> lock(state.mutex);
    state.expected = payload;
    state.received = false;
  }

  if (!client.send_blocking(payload)) {
    return {};
  }

  std::unique_lock<std::mutex> lock(state.mutex);
  return {true, state.cv.wait_for(lock, timeout,
                                  [&state] { return state.received; })};
}

SmokeResult run_payload(unilink::UdpClient& client, unilink::UdpServer& server,
                        EchoState& state,
                        const std::atomic<uint64_t>& server_received_total,
                        const std::atomic<uint64_t>& client_received_total,
                        const std::atomic<uint64_t>& echo_matches_total,
                        Strategy strategy, size_t payload_size,
                        size_t iterations, size_t warmup_iterations,
                        std::chrono::milliseconds timeout) {
  uint64_t warmup_consecutive_timeouts = 0;
  for (size_t i = 0; i < warmup_iterations; ++i) {
    const auto payload = make_payload(payload_size, i);
    const auto outcome = send_and_wait(client, state, payload, timeout);
    if (outcome.echoed) {
      warmup_consecutive_timeouts = 0;
    } else {
      ++warmup_consecutive_timeouts;
      if (warmup_consecutive_timeouts >= kMaxConsecutiveTimeouts) {
        break;
      }
    }
  }

  const auto server_before =
      server_received_total.load(std::memory_order_relaxed);
  const auto client_before =
      client_received_total.load(std::memory_order_relaxed);
  const auto matches_before =
      echo_matches_total.load(std::memory_order_relaxed);
  const auto client_stats_before = read_runtime_stats(client);
  const auto server_stats_before = read_runtime_stats(server);

  SmokeResult result;
  result.strategy = strategy_name(strategy);
  result.payload_size = payload_size;
  result.iterations = iterations;
  result.warmup_iterations = warmup_iterations;

  const auto start = Clock::now();
  uint64_t consecutive_timeouts = 0;
  for (size_t i = 0; i < iterations; ++i) {
    const auto payload = make_payload(payload_size, warmup_iterations + i);
    ++result.send_attempts;
    const auto outcome = send_and_wait(client, state, payload, timeout);
    if (outcome.accepted) {
      ++result.send_success;
      if (!outcome.echoed) {
        ++result.timeouts;
        ++consecutive_timeouts;
        if (consecutive_timeouts >= kMaxConsecutiveTimeouts) {
          result.stopped_early = true;
          result.stop_reason = "consecutive-timeouts";
          break;
        }
      } else {
        consecutive_timeouts = 0;
      }
    } else {
      ++result.send_failures;
      consecutive_timeouts = 0;
    }
  }
  const auto stop = Clock::now();
  const auto client_stats_after = read_runtime_stats(client);
  const auto server_stats_after = read_runtime_stats(server);

  result.server_received =
      server_received_total.load(std::memory_order_relaxed) - server_before;
  result.client_received =
      client_received_total.load(std::memory_order_relaxed) - client_before;
  result.echo_matches =
      echo_matches_total.load(std::memory_order_relaxed) - matches_before;

  result.delivery_percent =
      result.send_success == 0
          ? 0.0
          : 100.0 * static_cast<double>(result.client_received) /
                result.send_success;
  result.match_percent = result.send_success == 0
                             ? 0.0
                             : 100.0 *
                                   static_cast<double>(result.echo_matches) /
                                   result.send_success;
  result.elapsed_ms =
      std::chrono::duration<double, std::milli>(stop - start).count();
  result.messages_per_sec =
      result.elapsed_ms <= 0.0
          ? 0.0
          : 1000.0 * result.echo_matches / result.elapsed_ms;
  record_runtime_stats(result, client_stats_before, client_stats_after,
                       server_stats_before, server_stats_after);
  result.notes = notes_for(result);

  return result;
}

void run_strategy(const SmokeConfig& config, Strategy strategy, uint16_t port) {
  std::atomic<uint64_t> server_received_total{0};
  std::atomic<uint64_t> client_received_total{0};
  std::atomic<uint64_t> echo_matches_total{0};
  EchoState state;

  std::unique_ptr<unilink::UdpServer> server;
  server = std::make_unique<unilink::UdpServer>(port);
  server->backpressure_strategy(strategy);
  server->on_data([&](const unilink::MessageContext& ctx) {
    server_received_total.fetch_add(1, std::memory_order_relaxed);
    if (!server->send_to(ctx.client_id(), ctx.data())) {
      std::cerr << "[udp smoke server] echo send_to failed\n";
    }
  });
  server->on_error([](const unilink::ErrorContext& ctx) {
    std::cerr << "[udp smoke server] " << ctx.message() << "\n";
  });

  if (!server->start_sync()) {
    throw std::runtime_error("failed to start UDP payload smoke server");
  }

  unilink::config::UdpConfig client_config;
  client_config.local_port = 0;
  client_config.remote_address = config.host;
  client_config.remote_port = port;
  client_config.backpressure_strategy = strategy;

  unilink::UdpClient client(client_config);
  client.backpressure_strategy(strategy);
  client.on_data([&](const unilink::MessageContext& ctx) {
    std::lock_guard<std::mutex> lock(state.mutex);
    client_received_total.fetch_add(1, std::memory_order_relaxed);
    if (ctx.data() == state.expected) {
      echo_matches_total.fetch_add(1, std::memory_order_relaxed);
    }
    state.received = true;
    state.cv.notify_one();
  });
  client.on_error([](const unilink::ErrorContext& ctx) {
    std::cerr << "[udp smoke client] " << ctx.message() << "\n";
  });

  if (!client.start_sync()) {
    server->stop();
    throw std::runtime_error("failed to start UDP payload smoke client");
  }

  std::cout << "udp payload smoke: strategy=" << strategy_name(strategy)
            << " port=" << port << "\n";
  const auto timeout = std::chrono::milliseconds(config.timeout_ms);
  for (const auto payload_size : config.payload_sizes) {
    const auto result = run_payload(
        client, *server, state, server_received_total, client_received_total,
        echo_matches_total, strategy, payload_size, config.iterations,
        config.warmup_iterations, timeout);
    append_csv(config.csv_output, result);
    std::cout << "  payload=" << std::setw(6) << payload_size
              << " delivery=" << std::fixed << std::setprecision(2)
              << result.delivery_percent << "% match=" << result.match_percent
              << "% timeouts=" << result.timeouts << " notes=" << result.notes
              << "\n";
  }

  client.stop();
  server->stop();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const auto config = parse_args(argc, argv);
    const auto strategies = strategies_for(config.strategy);

    for (size_t i = 0; i < strategies.size(); ++i) {
      const auto port = static_cast<uint16_t>(config.port + i);
      run_strategy(config, strategies[i], port);
    }

    std::cout << "csv_output: " << config.csv_output << "\n";
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << "\n";
    return 1;
  }
}
