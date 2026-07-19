#include <algorithm>
#include <array>
#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "common/bench_config.hpp"

namespace {

using boost::asio::ip::udp;
using Clock = std::chrono::steady_clock;
constexpr uint64_t kMaxConsecutiveTimeouts = 10;

struct SmokeConfig {
  std::string host = wirestead_bench::kDefaultHost;
  uint16_t port = 9501;
  std::vector<size_t> payload_sizes{1024, 4096, 16384};
  size_t iterations = 1000;
  size_t warmup_iterations = 100;
  int timeout_ms = 1000;
  std::string csv_output = "build/raw_udp_payload_smoke.csv";
};

struct SmokeResult {
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
  double delivery_percent = 0.0;
  double match_percent = 0.0;
  double elapsed_ms = 0.0;
  double messages_per_sec = 0.0;
  std::string notes = "ok";
};

struct SendOutcome {
  bool sent = false;
  bool received = false;
  bool matched = false;
};

std::vector<size_t> parse_payload_sizes(const std::string& value) {
  std::istringstream input(value);
  std::vector<size_t> sizes;
  std::string token;
  while (input >> token) {
    sizes.push_back(wirestead_bench::parse_size(token, "payload size"));
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
      << "       [--csv-output PATH]\n";
}

SmokeConfig parse_args(int argc, char** argv) {
  SmokeConfig config;
  for (int i = 1; i < argc; ++i) {
    std::string value;
    const std::string arg = argv[i];
    if (arg == "--host" && wirestead_bench::read_option(argc, argv, i, &value)) {
      config.host = value;
    } else if (arg == "--port" &&
               wirestead_bench::read_option(argc, argv, i, &value)) {
      config.port = wirestead_bench::parse_port(value);
    } else if (arg == "--payload-sizes" &&
               wirestead_bench::read_option(argc, argv, i, &value)) {
      config.payload_sizes = parse_payload_sizes(value);
    } else if (arg == "--iterations" &&
               wirestead_bench::read_option(argc, argv, i, &value)) {
      config.iterations = wirestead_bench::parse_size(value, "iterations");
    } else if (arg == "--warmup-iterations" &&
               wirestead_bench::read_option(argc, argv, i, &value)) {
      config.warmup_iterations =
          wirestead_bench::parse_nonnegative_size(value, "warmup iterations");
    } else if (arg == "--timeout-ms" &&
               wirestead_bench::read_option(argc, argv, i, &value)) {
      config.timeout_ms = wirestead_bench::parse_positive_int(value, "timeout");
    } else if (arg == "--csv-output" &&
               wirestead_bench::read_option(argc, argv, i, &value)) {
      config.csv_output = value;
    } else {
      print_usage(argv[0]);
      throw std::invalid_argument("unknown or incomplete option: " + arg);
    }
  }
  return config;
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
  if (result.send_attempts < result.iterations) {
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
    output << "transport,payload_size,iterations,warmup_iterations,send_"
              "attempts,send_success,"
              "send_failures,server_received,client_received,echo_matches,"
              "timeouts,delivery_percent,"
              "match_percent,elapsed_ms,messages_per_sec,notes\n";
  }

  output << "raw_udp," << result.payload_size << "," << result.iterations << ","
         << result.warmup_iterations << "," << result.send_attempts << ","
         << result.send_success << "," << result.send_failures << ","
         << result.server_received << "," << result.client_received << ","
         << result.echo_matches << "," << result.timeouts << "," << std::fixed
         << std::setprecision(6) << result.delivery_percent << ","
         << result.match_percent << "," << result.elapsed_ms << ","
         << result.messages_per_sec << "," << result.notes << "\n";
}

class RawUdpEchoServer {
 public:
  RawUdpEchoServer(boost::asio::io_context& io, uint16_t port)
      : socket_(io, udp::endpoint(udp::v4(), port)) {
    start_receive();
  }

  uint64_t received() const {
    return received_.load(std::memory_order_relaxed);
  }

 private:
  void start_receive() {
    socket_.async_receive_from(
        boost::asio::buffer(buffer_), sender_,
        [this](const boost::system::error_code& error,
               std::size_t bytes_transferred) {
          if (!error) {
            received_.fetch_add(1, std::memory_order_relaxed);
            auto echo_payload = std::make_shared<std::string>(
                buffer_.data(), bytes_transferred);
            socket_.async_send_to(
                boost::asio::buffer(*echo_payload), sender_,
                [echo_payload](const boost::system::error_code&, std::size_t) {
                });
          }
          start_receive();
        });
  }

  udp::socket socket_;
  udp::endpoint sender_;
  std::array<char, 65507> buffer_{};
  std::atomic<uint64_t> received_{0};
};

SendOutcome send_and_wait(boost::asio::io_context& io, udp::socket& socket,
                          const udp::endpoint& server_endpoint,
                          const std::string& payload,
                          std::chrono::milliseconds timeout) {
  boost::system::error_code send_error;
  const auto sent_bytes = socket.send_to(boost::asio::buffer(payload),
                                         server_endpoint, 0, send_error);
  if (send_error || sent_bytes != payload.size()) {
    return {};
  }

  std::array<char, 65507> receive_buffer{};
  udp::endpoint sender;
  boost::asio::steady_timer timer(io);
  SendOutcome outcome;
  outcome.sent = true;

  socket.async_receive_from(
      boost::asio::buffer(receive_buffer), sender,
      [&](const boost::system::error_code& error,
          std::size_t bytes_transferred) {
        if (!error) {
          outcome.received = true;
          outcome.matched =
              bytes_transferred == payload.size() &&
              std::equal(payload.begin(), payload.end(), receive_buffer.begin(),
                         receive_buffer.begin() + bytes_transferred);
        }
        timer.cancel();
      });

  timer.expires_after(timeout);
  timer.async_wait([&](const boost::system::error_code& error) {
    if (!error) {
      socket.cancel();
    }
  });

  io.restart();
  io.run();
  return outcome;
}

SmokeResult run_payload(boost::asio::io_context& client_io,
                        udp::socket& client_socket,
                        const udp::endpoint& server_endpoint,
                        const RawUdpEchoServer& server, size_t payload_size,
                        size_t iterations, size_t warmup_iterations,
                        std::chrono::milliseconds timeout) {
  uint64_t warmup_consecutive_timeouts = 0;
  for (size_t i = 0; i < warmup_iterations; ++i) {
    const auto payload = make_payload(payload_size, i);
    const auto outcome = send_and_wait(client_io, client_socket,
                                       server_endpoint, payload, timeout);
    if (outcome.received) {
      warmup_consecutive_timeouts = 0;
    } else {
      ++warmup_consecutive_timeouts;
      if (warmup_consecutive_timeouts >= kMaxConsecutiveTimeouts) {
        break;
      }
    }
  }

  const auto server_before = server.received();

  SmokeResult result;
  result.payload_size = payload_size;
  result.iterations = iterations;
  result.warmup_iterations = warmup_iterations;

  const auto start = Clock::now();
  uint64_t consecutive_timeouts = 0;
  for (size_t i = 0; i < iterations; ++i) {
    const auto payload = make_payload(payload_size, warmup_iterations + i);
    ++result.send_attempts;
    const auto outcome = send_and_wait(client_io, client_socket,
                                       server_endpoint, payload, timeout);
    if (outcome.sent) {
      ++result.send_success;
      if (outcome.received) {
        ++result.client_received;
        consecutive_timeouts = 0;
      } else {
        ++result.timeouts;
        ++consecutive_timeouts;
        if (consecutive_timeouts >= kMaxConsecutiveTimeouts) {
          break;
        }
      }
      if (outcome.matched) {
        ++result.echo_matches;
      }
    } else {
      ++result.send_failures;
      consecutive_timeouts = 0;
    }
  }
  const auto stop = Clock::now();

  result.server_received = server.received() - server_before;
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
  result.notes = notes_for(result);

  return result;
}

void run_smoke(const SmokeConfig& config) {
  boost::asio::io_context server_io;
  RawUdpEchoServer server(server_io, config.port);
  std::thread server_thread([&] { server_io.run(); });

  boost::asio::io_context client_io;
  udp::socket client_socket(client_io, udp::endpoint(udp::v4(), 0));
  udp::resolver resolver(client_io);
  const auto resolved =
      resolver.resolve(udp::v4(), config.host, std::to_string(config.port));
  const auto server_endpoint = resolved.begin()->endpoint();

  std::cout << "raw udp payload smoke: port=" << config.port << "\n";
  const auto timeout = std::chrono::milliseconds(config.timeout_ms);
  for (const auto payload_size : config.payload_sizes) {
    const auto result = run_payload(client_io, client_socket, server_endpoint,
                                    server, payload_size, config.iterations,
                                    config.warmup_iterations, timeout);
    append_csv(config.csv_output, result);
    std::cout << "  payload=" << std::setw(6) << payload_size
              << " delivery=" << std::fixed << std::setprecision(2)
              << result.delivery_percent << "% match=" << result.match_percent
              << "% timeouts=" << result.timeouts << " notes=" << result.notes
              << "\n";
  }

  client_socket.close();
  server_io.stop();
  server_thread.join();
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const auto config = parse_args(argc, argv);
    run_smoke(config);
    std::cout << "csv_output: " << config.csv_output << "\n";
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << "\n";
    return 1;
  }
}
