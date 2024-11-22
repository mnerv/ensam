/**
 * Ensamble Part
 */
#include <vector>
#include <string_view>

#include <fcntl.h>

#include "fmt/format.h"
#include "asio.hpp"

class porter {
public:
    porter(asio::io_context &io) : m_context(io), m_socket(io) {}

    auto socket() -> asio::ip::tcp::socket& { return m_socket; }

    auto connect(std::string_view const& host, std::string_view const& port) -> void {
        asio::ip::tcp::resolver resolver{m_context};
        asio::async_connect(m_socket, resolver.resolve(host, port), std::bind(&porter::on_connect, this, std::placeholders::_1));
    }

private:
    auto on_connect([[maybe_unused]]asio::error_code const& ec) -> void {
        fmt::print("connected!\n");
        std::string text = "Hello, World!";
        asio::async_write(m_socket, asio::buffer(text), std::bind(&porter::on_write, this));
    }

    auto on_read() -> void {}

    auto on_write() -> void {
        // m_socket.close();
        // m_context.stop();
    }

    auto reader() -> void {
    }

private:
    asio::io_context &m_context;
    asio::ip::tcp::socket m_socket;
};

static auto entry([[maybe_unused]]std::vector<std::string_view> const& args) -> void {
    asio::io_context ctx{};
    asio::signal_set sig(ctx, SIGINT, SIGTERM);
    sig.async_wait([&] (auto, auto) { ctx.stop(); });

    porter p{ctx};
    p.connect("localhost", "1337");

    ctx.run();
}

auto main([[maybe_unused]]int argc, [[maybe_unused]]char const* argv[]) -> int {
    try {
        entry({argv, std::next(argv, argc)});
    } catch (std::exception const& e) {
        fmt::print(stderr, "entry: {}\n", e.what());
        return 1;
    }
    return 0;
}

