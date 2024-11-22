/**
 * Maestro, conductor
 */
#include <vector>
#include <string_view>
#include <functional>
#include <chrono>
#include <array>

#include "fmt/format.h"
#include "asio.hpp"

#include "ens/utils.hpp"

using namespace std::chrono_literals;

class conn {
public:
    conn(asio::io_context &io)
        : m_socket(io) {}

    auto socket() -> asio::ip::tcp::socket& { return m_socket; }

    auto start() -> void {
        reader();
    }

    auto stop() -> void {
        m_socket.close();
    }

private:
    auto reader() -> void {
        m_socket.async_read_some(asio::buffer(m_buffer_in),
            [&](asio::error_code const& ec, std::size_t len) {
            if (ec) {
                reader();
                return;
            }
            (void)len;
            m_msg = std::string(std::begin(m_buffer_in), std::end(m_buffer_in));

            fmt::print("msg: {}\n", m_msg);
            reader();
        });
    }

private:
    asio::ip::tcp::socket m_socket;
    std::array<std::uint8_t, 256> m_buffer_in{};
    std::string m_msg{};
};

class serv {
public:
    serv(asio::io_context &io, std::uint16_t port = 1337)
        : m_io(io)
        , m_acceptor(m_io, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)) { }

    auto listen() -> void {
        using namespace asio::ip;
        auto new_conn = std::make_shared<conn>(m_io);
        m_acceptor.async_accept(new_conn->socket(),
                                std::bind(&serv::listener, this, new_conn, asio::placeholders::error));
    }

private:
    auto listener(std::shared_ptr<conn> conn, std::error_code const& ec) -> void {
        if (!ec) {
            fmt::print("new connection!\n");
            fmt::print("    ip: {}:{}\n", conn->socket().remote_endpoint().address().to_v4().to_string(), conn->socket().remote_endpoint().port());

            conn->start();
            m_conns.push_back(conn);
        }
        listen();
    }

private:
    asio::io_context& m_io;
    asio::ip::tcp::acceptor m_acceptor;
    std::vector<std::shared_ptr<conn>> m_conns{};
};

static auto entry([[maybe_unused]]std::vector<std::string_view> const& args) -> void {
    asio::io_context ctx{};
    asio::signal_set sig(ctx, SIGINT, SIGTERM);
    sig.async_wait([&] (auto, auto) { ctx.stop(); });

    serv server(ctx);
    server.listen();

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

