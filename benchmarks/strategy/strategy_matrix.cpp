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
#include "unilink/unilink.hpp"

namespace {

using Strategy = unilink::base::constants::BackpressureStrategy;

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
              "received_bytes,accepted_mib_sec,received_mib_sec,delivery_rate\n";
  }

  output << result.transport << "," << result.strategy << "," << result.payload_size << ","
         << result.duration_seconds << "," << result.accepted_messages << "," << result.failed_sends << ","
         << result.accepted_bytes << "," << result.received_bytes << "," << std::fixed << std::setprecision(6)
         << result.accepted_mib_per_sec << "," << result.received_mib_per_sec << "," << result.delivery_rate << "\n";
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

StrategyResult run_tcp(const unilink_bench::StrategyConfig& config, Strategy strategy) {
  std::atomic<uint64_t> received_bytes{0};

  unilink::TcpServer server(config.tcp_port);
  server.on_data([&](const unilink::MessageContext& ctx) {
    received_bytes.fetch_add(ctx.data().size(), std::memory_order_relaxed);
  });
  server.on_error([](const unilink::ErrorContext& ctx) { std::cerr << "[tcp server] " << ctx.message() << "\n"; });

  if (!server.start_sync()) {
    throw std::runtime_error("failed to start TCP strategy server");
  }

  unilink::TcpClient client(config.host, config.tcp_port);
  client.backpressure_strategy(strategy);
  client.on_error([](const unilink::ErrorContext& ctx) { std::cerr << "[tcp client] " << ctx.message() << "\n"; });

  if (!client.start_sync()) {
    server.stop();
    throw std::runtime_error("failed to start TCP strategy client");
  }

  auto result =
      run_send_loop("tcp", client, strategy, config.payload_size, config.duration_seconds, received_bytes);
  client.stop();
  server.stop();
  return result;
}

StrategyResult run_udp(const unilink_bench::StrategyConfig& config, Strategy strategy) {
  std::atomic<uint64_t> received_bytes{0};

  unilink::UdpServer server(config.udp_server_port);
  server.on_data([&](const unilink::MessageContext& ctx) {
    received_bytes.fetch_add(ctx.data().size(), std::memory_order_relaxed);
  });
  server.on_error([](const unilink::ErrorContext& ctx) { std::cerr << "[udp server] " << ctx.message() << "\n"; });

  if (!server.start_sync()) {
    throw std::runtime_error("failed to start UDP strategy server");
  }

  unilink::config::UdpConfig client_config;
  client_config.local_port = config.udp_client_port;
  client_config.remote_address = config.host;
  client_config.remote_port = config.udp_server_port;

  unilink::UdpClient client(client_config);
  client.backpressure_strategy(strategy);
  client.on_error([](const unilink::ErrorContext& ctx) { std::cerr << "[udp client] " << ctx.message() << "\n"; });

  if (!client.start_sync()) {
    server.stop();
    throw std::runtime_error("failed to start UDP strategy client");
  }

  auto result =
      run_send_loop("udp", client, strategy, config.payload_size, config.duration_seconds, received_bytes);
  client.stop();
  server.stop();
  return result;
}

StrategyResult run_uds(const unilink_bench::StrategyConfig& config, Strategy strategy) {
  std::atomic<uint64_t> received_bytes{0};
  const std::string path = config.uds_path + "_" + strategy_name(strategy);
  std::filesystem::remove(path);

  unilink::UdsServer server(path);
  server.on_data([&](const unilink::MessageContext& ctx) {
    received_bytes.fetch_add(ctx.data().size(), std::memory_order_relaxed);
  });
  server.on_error([](const unilink::ErrorContext& ctx) { std::cerr << "[uds server] " << ctx.message() << "\n"; });

  if (!server.start_sync()) {
    throw std::runtime_error("failed to start UDS strategy server");
  }

  unilink::UdsClient client(path);
  client.backpressure_strategy(strategy);
  client.on_error([](const unilink::ErrorContext& ctx) { std::cerr << "[uds client] " << ctx.message() << "\n"; });

  if (!client.start_sync()) {
    server.stop();
    std::filesystem::remove(path);
    throw std::runtime_error("failed to start UDS strategy client");
  }

  auto result =
      run_send_loop("uds", client, strategy, config.payload_size, config.duration_seconds, received_bytes);
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
    const auto config = unilink_bench::parse_strategy_args(argc, argv);
    const Strategy strategies[] = {Strategy::Reliable, Strategy::BestEffort};

    std::cout << "payload_size: " << config.payload_size << " bytes\n";
    std::cout << "duration: " << config.duration_seconds << " s\n\n";
    std::cout << "| transport | strategy   | accepted MiB/s | received MiB/s | delivery% | failed sends |\n";
    std::cout << "|-----------|------------|----------------|----------------|-----------|--------------|\n";

    for (const auto strategy : strategies) {
      const auto tcp = run_tcp(config, strategy);
      print_result(tcp);
      if (config.csv_output) append_strategy_csv(*config.csv_output, tcp);

      const auto udp = run_udp(config, strategy);
      print_result(udp);
      if (config.csv_output) append_strategy_csv(*config.csv_output, udp);

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
