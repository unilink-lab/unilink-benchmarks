#include <exception>
#include <filesystem>
#include <iostream>

#include "common/bench_config.hpp"
#include "common/signal_wait.hpp"
#include "wirestead_bench_target.hpp"

int main(int argc, char** argv) {
  try {
    const auto path = wirestead_bench::parse_uds_server_args(argc, argv);
    std::filesystem::remove(path);

    std::unique_ptr<wirestead::UdsServer> server;
    server =
        wirestead::uds_server(path)
            .on_data([&server](const wirestead::MessageContext& ctx) { server->send_to(ctx.client_id(), ctx.data()); })
            .on_error([](const wirestead::ErrorContext& ctx) { std::cerr << "[error] " << ctx.message() << "\n"; })
            .build();

    if (!server->start_sync()) {
      std::cerr << "Failed to start UDS echo server on " << path << "\n";
      return 1;
    }

    std::cout << "UDS echo server listening on " << path << "\n";
    wirestead_bench::install_signal_handlers();
    wirestead_bench::wait_for_stop_signal();
    server->stop();
    std::filesystem::remove(path);
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << "\n";
    return 1;
  }
}
