#pragma once
#include <cstdint>
using byte = std::uint8_t;
using word = std::uint16_t;
namespace rxpce_machine {
struct Register16 {
    std::uint16_t value = 0;
    constexpr std::uint8_t low() const noexcept {
        return static_cast<std::uint8_t>(value & 0xff);
    }
    constexpr std::uint8_t high() const noexcept {
        return static_cast<std::uint8_t>(value >> 8);
    }
    constexpr void set_low(std::uint8_t next) noexcept {
        value = static_cast<std::uint16_t>((value & 0xff00) | next);
    }
    constexpr void set_high(std::uint8_t next) noexcept {
        value = static_cast<std::uint16_t>((value & 0xff) | (std::uint16_t(next) << 8));
    }
};
} // namespace rxpce_machine
