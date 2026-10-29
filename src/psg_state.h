#pragma once
#include <array>
#include <cstdint>

namespace rxpce_audio {
struct PsgState {
    std::uint8_t volume = 0;
    std::uint8_t lfo_frequency = 0, lfo_control = 0;
    std::array<std::array<std::uint8_t, 8>, 6> registers{};
    std::array<std::array<std::uint8_t, 32>, 6> wave{};
};
} // namespace rxpce_audio
