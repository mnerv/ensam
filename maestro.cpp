/**
 * Maestro, conductor
 */
#include <vector>
#include <string_view>

#include "fmt/format.h"

static auto entry([[maybe_unused]]std::vector<std::string_view> const& args) -> void {
    fmt::print("Hello, World!\n");
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

