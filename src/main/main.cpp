#include <iostream>
#include <thread>
#include <asio.hpp>
#include <spdlog/spdlog.h>

int main(int argc, char** argv) {
    spdlog::info("OmegaStreamer v0.1.0 Starting...");


    asio::io_context io_context;

    asio::steady_timer timer(io_context, asio::chrono::seconds(3));

    timer.async_wait([](const asio::error_code& ec) {
       if (!ec) {
           spdlog::info("Hello! 3 seconds have passed. ASIO is working.");
       } else {
           spdlog::error("Timer error: {}", ec.message());
       }
    });

    spdlog::info("IO Context is running on main thread...");

    io_context.run();

    spdlog::info("Server stopped.");
    return 0;
}