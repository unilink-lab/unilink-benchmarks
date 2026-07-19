#include <exception>
#include <iostream>

#include "common/bench_config.hpp"
#include "common/signal_wait.hpp"
#include "wirestead_bench_target.hpp"

int main(int argc, char** argv) {
  try {
    const auto port = wirestead_bench::parse_server_port_args(argc, argv, wirestead_bench::kDefaultTcpPort);
    std::unique_ptr<wirestead::TcpServer> server;

    server =
        wirestead::tcp_server(port)
            .on_data([&server](const wirestead::MessageContext& ctx) { server->send_to(ctx.client_id(), ctx.data()); })
            .on_error([](const wirestead::ErrorContext& ctx) { std::cerr << "[error] " << ctx.message() << "\n"; })
            .build();

    if (!server->start_sync()) {
      std::cerr << "Failed to start TCP echo server on port " << port << "\n";
      return 1;
    }

    std::cout << "TCP echo server listening on port " << port << "\n";
    wirestead_bench::install_signal_handlers();
    wirestead_bench::wait_for_stop_signal();
    server->stop();
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << "\n";
    return 1;
  }
}
