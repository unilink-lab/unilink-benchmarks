#include <exception>
#include <iostream>
#include <memory>
#include <string>

#include "common/bench_config.hpp"
#include "common/latency_runner.hpp"
#include "unilink/unilink.hpp"

class TcpBenchClient {
 public:
  explicit TcpBenchClient(const unilink_bench::NetworkConfig& config) {
    client_ =
        unilink::tcp_client(config.host, config.port)
            .on_data([this](const unilink::MessageContext& ctx) { waiter_.on_bytes(ctx.data()); })
            .on_error([this](const unilink::ErrorContext& ctx) { waiter_.on_error(ctx.message()); })
            .max_retries(0)
            .build();
  }

  bool start_sync() { return client_->start_sync(); }
  bool send_frame(std::string_view frame) { return client_->send_blocking(frame); }
  void stop() { client_->stop(); }
  unilink_bench::EchoWaiter& echo_waiter() { return waiter_; }

 private:
  unilink_bench::EchoWaiter waiter_;
  std::unique_ptr<unilink::TcpClient> client_;
};

int main(int argc, char** argv) {
  try {
    const auto config = unilink_bench::parse_network_client_args(argc, argv, unilink_bench::kDefaultTcpPort);
    TcpBenchClient client(config);
    return unilink_bench::run_latency_client("tcp", client, config.payload_size, config.iterations,
                                             config.warmup_iterations, config.csv_output);
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << "\n";
    return 1;
  }
}
