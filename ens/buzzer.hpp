#ifndef ENS_BUZZER_HPP
#define ENS_BUZZER_HPP

#include <chrono>

#include "midi.hpp"
#include "drv/sysfs.hpp"

namespace ens {
class buzzer {
public:
    buzzer();
    ~buzzer();

    auto play(ens::pitch p, std::int8_t octave, std::chrono::duration<std::chrono::milliseconds> t) -> void;

private:
    sysfs m_buzzer;
};
}

#endif  // !ENS_DRV_BUZZER_HPP
