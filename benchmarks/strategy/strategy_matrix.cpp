#include <atomic>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

#include "common/bench_config.hpp"
#include "wirestead_bench_target.hpp"

namespace {

using Strategy = wirestead::base::constants::BackpressureStrategy;

// Maximum UDP payload deliverable in a single IPv4 datagram: 65535 (max IP
// packet) - 20 (IP header) - 8 (UDP header). A payload of exactly 65536
// bytes exceeds this by 29 bytes and is rejected by the OS socket layer
// (EMSGSIZE) on any networking stack, regardless of the library used - not
// a unilink-specific limitation, so it's excluded from the UDP sweep
// rather than reported as a perpetual 0%-delivery result.
constexpr size_t kMaxUdpDatagramPayloadBytes = 65507;

struct StrategyResult {
  std::string transport;
  std::string strategy;
  size_t payload_size = 0;
  int duration_seconds = 0;
  uint64_t accepted_messages = 0;
  uint64_t failed_sends = 0;
  uint64_t accepted_bytes = 0;
  uint64_t received_bytes = 0;
  double accepted_mib_per_sec = 0.0;
  double received_mib_per_sec = 0.0;
  double delivery_rate = 0.0;
  bool runtime_stats_supported = false;
  uint64_t client_messages_accepted = 0;
  uint64_t client_bytes_accepted = 0;
  uint64_t client_messages_sent = 0;
  uint64_t client_bytes_sent = 0;
  uint64_t client_failed_sends = 0;
  uint64_t client_dropped_messages = 0;
  uint64_t client_dropped_bytes = 0;
  uint64_t client_backpressure_events = 0;
  size_t client_max_queued_bytes = 0;
  size_t client_queued_bytes_final = 0;
  size_t client_pending_bytes_final = 0;
  bool client_backpressure_active_final = false;
  uint64_t server_messages_received = 0;
  uint64_t server_bytes_received = 0;
  uint64_t server_failed_sends = 0;
  uint64_t server_dropped_messages = 0;
  uint64_t server_dropped_bytes = 0;
  uint64_t server_backpressure_events = 0;
  size_t server_max_queued_bytes = 0;
  size_t server_queued_bytes_final = 0;
  size_t server_pending_bytes_final = 0;
  bool server_backpressure_active_final = false;
};

std::string strategy_name(Strategy strategy) {
  return strategy == Strategy::Reliable ? "reliable" : "besteffort";
}

StrategyResult make_result(std::string transport, Strategy strategy, size_t payload_size, int duration_seconds,
                           uint64_t accepted_messages, uint64_t failed_sends, uint64_t accepted_bytes,
                           uint64_t received_bytes) {
  const double duration = static_cast<double>(duration_seconds);
  const double accepted_mib = static_cast<double>(accepted_bytes) / (1024.0 * 1024.0);
  const double received_mib = static_cast<double>(received_bytes) / (1024.0 * 1024.0);
  const double delivery_rate =
      accepted_bytes == 0 ? 0.0 : 100.0 * static_cast<double>(received_bytes) / static_cast<double>(accepted_bytes);

  return {std::move(transport),
          strategy_name(strategy),
          payload_size,
          duration_seconds,
          accepted_messages,
          failed_sends,
          accepted_bytes,
          received_bytes,
          accepted_mib / duration,
          received_mib / duration,
          delivery_rate};
}

#ifdef WIRESTEAD_BENCH_HAS_RUNTIME_STATS
template <typename Client, typename Server>
void record_runtime_stats(StrategyResult& result, const Client& client, const Server& server) {
  const auto client_stats = client.stats();
  const auto server_stats = server.stats();
  result.runtime_stats_supported = true;

  result.client_messages_accepted = client_stats.messages_accepted;
  result.client_bytes_accepted = client_stats.bytes_accepted;
  result.client_messages_sent = client_stats.messages_sent;
  result.client_bytes_sent = client_stats.bytes_sent;
  result.client_failed_sends = client_stats.failed_sends;
  result.client_dropped_messages = client_stats.dropped_messages;
  result.client_dropped_bytes = client_stats.dropped_bytes;
  result.client_backpressure_events = client_stats.backpressure_events;
  result.client_max_queued_bytes = client_stats.max_queued_bytes;
  result.client_queued_bytes_final = client_stats.queued_bytes;
  result.client_pending_bytes_final = client_stats.pending_bytes;
  result.client_backpressure_active_final = client_stats.backpressure_active;

  result.server_messages_received = server_stats.messages_received;
  result.server_bytes_received = server_stats.bytes_received;
  result.server_failed_sends = server_stats.failed_sends;
  result.server_dropped_messages = server_stats.dropped_messages;
  result.server_dropped_bytes = server_stats.dropped_bytes;
  result.server_backpressure_events = server_stats.backpressure_events;
  result.server_max_queued_bytes = server_stats.max_queued_bytes;
  result.server_queued_bytes_final = server_stats.queued_bytes;
  result.server_pending_bytes_final = server_stats.pending_bytes;
  result.server_backpressure_active_final = server_stats.backpressure_active;
}
#else
template <typename Client, typename Server>
void record_runtime_stats(StrategyResult&, const Client&, const Server&) {}
#endif

bool file_is_empty_or_missing(const std::string& path) {
  std::ifstream input(path);
  return !input.good() || input.peek() == std::ifstream::traits_type::eof();
}

void append_strategy_csv(const std::string& path, const StrategyResult& result) {
  const bool write_header = file_is_empty_or_missing(path);
  std::ofstream output(path, std::ios::app);
  if (!output) {
    throw std::runtime_error("failed to open CSV output: " + path);
  }

  if (write_header) {
    output << "transport,strategy,payload_size,duration_s,accepted_messages,failed_sends,accepted_bytes,"
              "received_bytes,accepted_mib_sec,received_mib_sec,delivery_rate,runtime_stats_supported,"
              "client_messages_accepted,client_bytes_accepted,client_messages_sent,client_bytes_sent,"
              "client_failed_sends,client_dropped_messages,client_dropped_bytes,client_backpressure_events,"
              "client_max_queued_bytes,client_queued_bytes_final,client_pending_bytes_final,"
              "client_backpressure_active_final,server_messages_received,server_bytes_received,server_failed_sends,"
              "server_dropped_messages,server_dropped_bytes,server_backpressure_events,server_max_queued_bytes,"
              "server_queued_bytes_final,server_pending_bytes_final,server_backpressure_active_final\n";
  }

  output << result.transport << "," << result.strategy << "," << result.payload_size << ","
         << result.duration_seconds << "," << result.accepted_messages << "," << result.failed_sends << ","
         << result.accepted_bytes << "," << result.received_bytes << "," << std::fixed << std::setprecision(6)
         << result.accepted_mib_per_sec << "," << result.received_mib_per_sec << "," << result.delivery_rate << ","
         << (result.runtime_stats_supported ? 1 : 0) << "," << result.client_messages_accepted << ","
         << result.client_bytes_accepted << "," << result.client_messages_sent << "," << result.client_bytes_sent
         << "," << result.client_failed_sends << "," << result.client_dropped_messages << ","
         << result.client_dropped_bytes << "," << result.client_backpressure_events << ","
         << result.client_max_queued_bytes << "," << result.client_queued_bytes_final << ","
         << result.client_pending_bytes_final << "," << (result.client_backpressure_active_final ? 1 : 0) << ","
         << result.server_messages_received << "," << result.server_bytes_received << ","
         << result.server_failed_sends << "," << result.server_dropped_messages << ","
         << result.server_dropped_bytes << "," << result.server_backpressure_events << ","
         << result.server_max_queued_bytes << "," << result.server_queued_bytes_final << ","
         << result.server_pending_bytes_final << "," << (result.server_backpressure_active_final ? 1 : 0) << "\n";
}

template <typename Client>
StrategyResult run_send_loop(std::string transport, Client& client, Strategy strategy, size_t payload_size,
                             int duration_seconds, const std::atomic<uint64_t>& received_bytes) {
  const std::string payload(payload_size, 'A');
  std::atomic<bool> running{true};
  std::atomic<uint64_t> accepted_messages{0};
  std::atomic<uint64_t> failed_sends{0};
  std::atomic<uint64_t> accepted_bytes{0};

  std::thread sender([&] {
    while (running.load(std::memory_order_relaxed)) {
      if (client.send(payload)) {
        accepted_messages.fetch_add(1, std::memory_order_relaxed);
        accepted_bytes.fetch_add(payload.size(), std::memory_order_relaxed);
      } else {
        failed_sends.fetch_add(1, std::memory_order_relaxed);
        std::this_thread::sleep_for(std::chrono::microseconds(50));
      }
    }
  });

  std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));
  running.store(false, std::memory_order_relaxed);
  sender.join();

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  return make_result(std::move(transport), strategy, payload_size, duration_seconds, accepted_messages.load(),
                     failed_sends.load(), accepted_bytes.load(), received_bytes.load());
}

StrategyResult run_tcp(const wirestead_bench::StrategyConfig& config, Strategy strategy) {
  std::atomic<uint64_t> received_bytes{0};

  wirestead::TcpServer server(config.tcp_port);
  server.on_data([&](const wirestead::MessageContext& ctx) {
    received_bytes.fetch_add(ctx.data().size(), std::memory_order_relaxed);
  });
  server.on_error([](const wirestead::ErrorContext& ctx) { std::cerr << "[tcp server] " << ctx.message() << "\n"; });

  if (!server.start_sync()) {
    throw std::runtime_error("failed to start TCP strategy server");
  }

  wirestead::TcpClient client(config.host, config.tcp_port);
  client.backpressure_strategy(strategy);
  client.on_error([](const wirestead::ErrorContext& ctx) { std::cerr << "[tcp client] " << ctx.message() << "\n"; });

  if (!client.start_sync()) {
    server.stop();
    throw std::runtime_error("failed to start TCP strategy client");
  }

  auto result =
      run_send_loop("tcp", client, strategy, config.payload_size, config.duration_seconds, received_bytes);
  record_runtime_stats(result, client, server);
  client.stop();
  server.stop();
  return result;
}

StrategyResult run_udp(const wirestead_bench::StrategyConfig& config, Strategy strategy) {
  std::atomic<uint64_t> received_bytes{0};

  wirestead::UdpServer server(config.udp_server_port);
  server.on_data([&](const wirestead::MessageContext& ctx) {
    received_bytes.fetch_add(ctx.data().size(), std::memory_order_relaxed);
  });
  server.on_error([](const wirestead::ErrorContext& ctx) { std::cerr << "[udp server] " << ctx.message() << "\n"; });

  if (!server.start_sync()) {
    throw std::runtime_error("failed to start UDP strategy server");
  }

  wirestead::config::UdpConfig client_config;
  client_config.local_port = config.udp_client_port;
  client_config.remote_address = config.host;
  client_config.remote_port = config.udp_server_port;

  wirestead::UdpClient client(client_config);
  client.backpressure_strategy(strategy);
  client.on_error([](const wirestead::ErrorContext& ctx) { std::cerr << "[udp client] " << ctx.message() << "\n"; });

  if (!client.start_sync()) {
    server.stop();
    throw std::runtime_error("failed to start UDP strategy client");
  }

  auto result =
      run_send_loop("udp", client, strategy, config.payload_size, config.duration_seconds, received_bytes);
  record_runtime_stats(result, client, server);
  client.stop();
  server.stop();
  return result;
}

StrategyResult run_uds(const wirestead_bench::StrategyConfig& config, Strategy strategy) {
  std::atomic<uint64_t> received_bytes{0};
  const std::string path = config.uds_path + "_" + strategy_name(strategy);
  std::filesystem::remove(path);

  wirestead::UdsServer server(path);
  server.on_data([&](const wirestead::MessageContext& ctx) {
    received_bytes.fetch_add(ctx.data().size(), std::memory_order_relaxed);
  });
  server.on_error([](const wirestead::ErrorContext& ctx) { std::cerr << "[uds server] " << ctx.message() << "\n"; });

  if (!server.start_sync()) {
    throw std::runtime_error("failed to start UDS strategy server");
  }

  wirestead::UdsClient client(path);
  client.backpressure_strategy(strategy);
  client.on_error([](const wirestead::ErrorContext& ctx) { std::cerr << "[uds client] " << ctx.message() << "\n"; });

  if (!client.start_sync()) {
    server.stop();
    std::filesystem::remove(path);
    throw std::runtime_error("failed to start UDS strategy client");
  }

  auto result =
      run_send_loop("uds", client, strategy, config.payload_size, config.duration_seconds, received_bytes);
  record_runtime_stats(result, client, server);
  client.stop();
  server.stop();
  std::filesystem::remove(path);
  return result;
}

void print_result(const StrategyResult& result) {
  std::cout << "| " << std::left << std::setw(9) << result.transport << " | " << std::setw(10) << result.strategy
            << " | " << std::right << std::setw(12) << result.accepted_mib_per_sec << " | " << std::setw(12)
            << result.received_mib_per_sec << " | " << std::setw(9) << result.delivery_rate << " | "
            << std::setw(12) << result.failed_sends << " |\n";
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const auto config = wirestead_bench::parse_strategy_args(argc, argv);
    const Strategy strategies[] = {Strategy::Reliable, Strategy::BestEffort};

    std::cout << "payload_size: " << config.payload_size << " bytes\n";
    std::cout << "duration: " << config.duration_seconds << " s\n\n";
    std::cout << "| transport | strategy   | accepted MiB/s | received MiB/s | delivery% | failed sends |\n";
    std::cout << "|-----------|------------|----------------|----------------|-----------|--------------|\n";

    for (const auto strategy : strategies) {
      const auto tcp = run_tcp(config, strategy);
      print_result(tcp);
      if (config.csv_output) append_strategy_csv(*config.csv_output, tcp);

      if (config.payload_size <= kMaxUdpDatagramPayloadBytes) {
        const auto udp = run_udp(config, strategy);
        print_result(udp);
        if (config.csv_output) append_strategy_csv(*config.csv_output, udp);
      } else {
        std::cout << "| udp       | " << std::left << std::setw(10)
                   << (strategy == Strategy::Reliable ? "reliable" : "besteffort") << " | skipped: payload "
                   << config.payload_size << " exceeds max IPv4 UDP datagram payload ("
                   << kMaxUdpDatagramPayloadBytes << " bytes) |\n";
      }

      const auto uds = run_uds(config, strategy);
      print_result(uds);
      if (config.csv_output) append_strategy_csv(*config.csv_output, uds);
    }

    if (config.csv_output) {
      std::cout << "\ncsv_output: " << *config.csv_output << "\n";
    }

    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << "\n";
    return 1;
  }
}
