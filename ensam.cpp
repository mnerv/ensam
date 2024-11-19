/**
 * Ensamble Part
 */
#include <vector>
#include <string_view>

#include <fcntl.h>

#include "fmt/format.h"

//constexpr auto buzzer_sysfs_enable = "/sys/devices/platform/pwmmap/buzzer_pwm/enable";
//constexpr auto buzzer_sysfs_period = "/sys/devices/platform/pwmmap/buzzer_pwm/period";
//constexpr auto buzzer_sysfs_duty   = "/sys/devices/platform/pwmmap/buzzer_pwm/duty_cycle";

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

