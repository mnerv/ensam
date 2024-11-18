/**
 * Maestro, conductor
 */
#include <vector>
#include <string_view>
#include <functional>
#include <chrono>

#include "fmt/format.h"
#include "asio.hpp"

#include "ens/utils.hpp"

using namespace std::chrono_literals;

auto print(asio::error_code const& ec, asio::steady_timer* timer, asio::steady_timer::time_point& t0, std::chrono::milliseconds const interval) -> void {
    if (ec) return;
    auto const now = asio::steady_timer::clock_type::now();
    auto const dt = std::chrono::duration_cast<std::chrono::milliseconds>(now - t0);
    t0 = now;

    fmt::print("Hello, World! Elapsed: {} ms\n", dt.count());

    timer->expires_at(timer->expiry() + interval);
    timer->async_wait(std::bind(print, asio::placeholders::error, timer, std::ref(t0), interval));
}

static auto entry([[maybe_unused]]std::vector<std::string_view> const& args) -> void {
    asio::io_context ctx{};
    asio::signal_set sig(ctx, SIGINT, SIGTERM);
    sig.async_wait([&] (auto, auto) { ctx.stop(); });

    using namespace std::chrono_literals;
    auto const timer_interval = 16ms;
    auto t0 = asio::steady_timer::clock_type::now();
    asio::steady_timer timer(ctx, timer_interval);
    timer.async_wait(std::bind(print, asio::placeholders::error, &timer, std::ref(t0), timer_interval));

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

