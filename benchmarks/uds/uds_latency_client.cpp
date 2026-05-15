#include <exception>
#include <iostream>
#include <memory>
#include <string>

#include "common/bench_config.hpp"
#include "common/latency_runner.hpp"
#include "unilink/unilink.hpp"

class UdsBenchClient {
 public:
  explicit UdsBenchClient(const unilink_bench::UdsConfig& config) {
    client_ =
        unilink::uds_client(config.path)
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
  std::unique_ptr<unilink::UdsClient> client_;
};

int main(int argc, char** argv) {
  try {
    const auto config = unilink_bench::parse_uds_client_args(argc, argv);
    UdsBenchClient client(config);
    return unilink_bench::run_latency_client("uds", client, config.payload_size, config.iterations);
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << "\n";
    return 1;
  }
}
