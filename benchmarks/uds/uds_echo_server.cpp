#include <exception>
#include <filesystem>
#include <iostream>

#include "common/bench_config.hpp"
#include "common/signal_wait.hpp"
#include "unilink/unilink.hpp"

int main(int argc, char** argv) {
  try {
    const auto path = unilink_bench::parse_uds_server_args(argc, argv);
    std::filesystem::remove(path);

    std::unique_ptr<unilink::UdsServer> server;
    server =
        unilink::uds_server(path)
            .on_data([&server](const unilink::MessageContext& ctx) { server->send_to(ctx.client_id(), ctx.data()); })
            .on_error([](const unilink::ErrorContext& ctx) { std::cerr << "[error] " << ctx.message() << "\n"; })
            .build();

    if (!server->start_sync()) {
      std::cerr << "Failed to start UDS echo server on " << path << "\n";
      return 1;
    }

    std::cout << "UDS echo server listening on " << path << "\n";
    unilink_bench::install_signal_handlers();
    unilink_bench::wait_for_stop_signal();
    server->stop();
    std::filesystem::remove(path);
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << "\n";
    return 1;
  }
}
