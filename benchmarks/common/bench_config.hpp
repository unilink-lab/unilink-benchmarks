#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace unilink_bench {

constexpr const char* kDefaultHost = "127.0.0.1";
constexpr uint16_t kDefaultTcpPort = 9000;
constexpr uint16_t kDefaultUdpPort = 9001;
constexpr const char* kDefaultUdsPath = "/tmp/unilink_bench.sock";
constexpr size_t kDefaultPayloadSize = 1024;
constexpr size_t kDefaultIterations = 100000;
constexpr size_t kDefaultWarmupIterations = 0;
constexpr int kDefaultStrategyDurationSeconds = 3;

struct NetworkConfig {
  std::string host = kDefaultHost;
  uint16_t port = kDefaultTcpPort;
  size_t payload_size = kDefaultPayloadSize;
  size_t iterations = kDefaultIterations;
  size_t warmup_iterations = kDefaultWarmupIterations;
  std::optional<std::string> csv_output;
};

struct UdsConfig {
  std::string path = kDefaultUdsPath;
  size_t payload_size = kDefaultPayloadSize;
  size_t iterations = kDefaultIterations;
  size_t warmup_iterations = kDefaultWarmupIterations;
  std::optional<std::string> csv_output;
};

struct StrategyConfig {
  std::string host = kDefaultHost;
  uint16_t tcp_port = 9100;
  uint16_t udp_server_port = 9101;
  uint16_t udp_client_port = 9102;
  std::string uds_path = "/tmp/unilink_strategy_bench.sock";
  size_t payload_size = kDefaultPayloadSize;
  int duration_seconds = kDefaultStrategyDurationSeconds;
  std::optional<std::string> csv_output;
};

inline uint16_t parse_port(const std::string& value) {
  const auto port = std::stoul(value);
  if (port > 65535) {
    throw std::invalid_argument("port must be in range 0..65535");
  }
  return static_cast<uint16_t>(port);
}

inline size_t parse_size(const std::string& value, const char* name) {
  const auto parsed = std::stoull(value);
  if (parsed == 0) {
    throw std::invalid_argument(std::string(name) + " must be greater than zero");
  }
  return static_cast<size_t>(parsed);
}

inline size_t parse_nonnegative_size(const std::string& value, const char*) {
  return static_cast<size_t>(std::stoull(value));
}

inline int parse_positive_int(const std::string& value, const char* name) {
  const auto parsed = std::stoi(value);
  if (parsed <= 0) {
    throw std::invalid_argument(std::string(name) + " must be greater than zero");
  }
  return parsed;
}

inline bool read_option(int argc, char** argv, int& index, std::string* value) {
  if (index + 1 >= argc) {
    return false;
  }
  *value = argv[++index];
  return true;
}

inline void print_common_usage(const char* program) {
  std::cerr << "Usage: " << program << " [--host HOST] [--port PORT] [--path PATH]\n"
            << "       [--payload-size BYTES] [--iterations COUNT]\n"
            << "       [--warmup-iterations COUNT] [--csv-output PATH]\n";
}

inline NetworkConfig parse_network_client_args(int argc, char** argv, uint16_t default_port) {
  NetworkConfig config;
  config.port = default_port;

  for (int i = 1; i < argc; ++i) {
    std::string value;
    const std::string arg = argv[i];
    if (arg == "--host" && read_option(argc, argv, i, &value)) {
      config.host = value;
    } else if (arg == "--port" && read_option(argc, argv, i, &value)) {
      config.port = parse_port(value);
    } else if (arg == "--payload-size" && read_option(argc, argv, i, &value)) {
      config.payload_size = parse_size(value, "payload size");
    } else if (arg == "--iterations" && read_option(argc, argv, i, &value)) {
      config.iterations = parse_size(value, "iterations");
    } else if (arg == "--warmup-iterations" && read_option(argc, argv, i, &value)) {
      config.warmup_iterations = parse_nonnegative_size(value, "warmup iterations");
    } else if (arg == "--csv-output" && read_option(argc, argv, i, &value)) {
      config.csv_output = value;
    } else {
      print_common_usage(argv[0]);
      throw std::invalid_argument("unknown or incomplete option: " + arg);
    }
  }

  return config;
}

inline uint16_t parse_server_port_args(int argc, char** argv, uint16_t default_port) {
  uint16_t port = default_port;
  for (int i = 1; i < argc; ++i) {
    std::string value;
    const std::string arg = argv[i];
    if (arg == "--port" && read_option(argc, argv, i, &value)) {
      port = parse_port(value);
    } else {
      print_common_usage(argv[0]);
      throw std::invalid_argument("unknown or incomplete option: " + arg);
    }
  }
  return port;
}

inline UdsConfig parse_uds_client_args(int argc, char** argv) {
  UdsConfig config;
  for (int i = 1; i < argc; ++i) {
    std::string value;
    const std::string arg = argv[i];
    if (arg == "--path" && read_option(argc, argv, i, &value)) {
      config.path = value;
    } else if (arg == "--payload-size" && read_option(argc, argv, i, &value)) {
      config.payload_size = parse_size(value, "payload size");
    } else if (arg == "--iterations" && read_option(argc, argv, i, &value)) {
      config.iterations = parse_size(value, "iterations");
    } else if (arg == "--warmup-iterations" && read_option(argc, argv, i, &value)) {
      config.warmup_iterations = parse_nonnegative_size(value, "warmup iterations");
    } else if (arg == "--csv-output" && read_option(argc, argv, i, &value)) {
      config.csv_output = value;
    } else {
      print_common_usage(argv[0]);
      throw std::invalid_argument("unknown or incomplete option: " + arg);
    }
  }
  return config;
}

inline std::string parse_uds_server_args(int argc, char** argv) {
  std::string path = kDefaultUdsPath;
  for (int i = 1; i < argc; ++i) {
    std::string value;
    const std::string arg = argv[i];
    if (arg == "--path" && read_option(argc, argv, i, &value)) {
      path = value;
    } else {
      print_common_usage(argv[0]);
      throw std::invalid_argument("unknown or incomplete option: " + arg);
    }
  }
  return path;
}

inline StrategyConfig parse_strategy_args(int argc, char** argv) {
  StrategyConfig config;
  for (int i = 1; i < argc; ++i) {
    std::string value;
    const std::string arg = argv[i];
    if (arg == "--host" && read_option(argc, argv, i, &value)) {
      config.host = value;
    } else if (arg == "--tcp-port" && read_option(argc, argv, i, &value)) {
      config.tcp_port = parse_port(value);
    } else if (arg == "--udp-server-port" && read_option(argc, argv, i, &value)) {
      config.udp_server_port = parse_port(value);
    } else if (arg == "--udp-client-port" && read_option(argc, argv, i, &value)) {
      config.udp_client_port = parse_port(value);
    } else if (arg == "--uds-path" && read_option(argc, argv, i, &value)) {
      config.uds_path = value;
    } else if (arg == "--payload-size" && read_option(argc, argv, i, &value)) {
      config.payload_size = parse_size(value, "payload size");
    } else if (arg == "--duration" && read_option(argc, argv, i, &value)) {
      config.duration_seconds = parse_positive_int(value, "duration");
    } else if (arg == "--csv-output" && read_option(argc, argv, i, &value)) {
      config.csv_output = value;
    } else {
      std::cerr << "Usage: " << argv[0]
                << " [--host HOST] [--tcp-port PORT] [--udp-server-port PORT]\n"
                << "       [--udp-client-port PORT] [--uds-path PATH]\n"
                << "       [--payload-size BYTES] [--duration SECONDS] [--csv-output PATH]\n";
      throw std::invalid_argument("unknown or incomplete option: " + arg);
    }
  }
  return config;
}

}  // namespace unilink_bench
