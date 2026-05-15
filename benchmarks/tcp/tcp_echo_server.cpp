#include <exception>
#include <iostream>

#include "common/bench_config.hpp"
#include "common/signal_wait.hpp"
#include "unilink/unilink.hpp"

int main(int argc, char** argv) {
  try {
    const auto port = unilink_bench::parse_server_port_args(argc, argv, unilink_bench::kDefaultTcpPort);
    std::unique_ptr<unilink::TcpServer> server;

    server =
        unilink::tcp_server(port)
            .on_data([&server](const unilink::MessageContext& ctx) { server->send_to(ctx.client_id(), ctx.data()); })
            .on_error([](const unilink::ErrorContext& ctx) { std::cerr << "[error] " << ctx.message() << "\n"; })
            .build();

    if (!server->start_sync()) {
      std::cerr << "Failed to start TCP echo server on port " << port << "\n";
      return 1;
    }

    std::cout << "TCP echo server listening on port " << port << "\n";
    unilink_bench::install_signal_handlers();
    unilink_bench::wait_for_stop_signal();
    server->stop();
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << "\n";
    return 1;
  }
}
