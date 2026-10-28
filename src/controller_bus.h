#pragma once
#include "input_state.h"

namespace rxpce_input {
struct ControllerBus {
    std::array<std::uint16_t, 5> buttons{};
    std::array<std::uint8_t, 5> modes{{2, 2, 2, 2, 2}}, phase{};
    std::uint8_t count = 1, port = 0, sel = 0, clr = 0;
    void configure(unsigned connected, const std::array<std::uint8_t, 5>& types) {
        *this = ControllerBus{};
        count = static_cast<std::uint8_t>(connected);
        modes = types;
        if (count > 1)
            port = 5; // No tap port is selected until the first CLR reset.
    }
    void activate() {
        if (port < count && modes[port] == 6)
            phase[port] ^= 1;
    }
    void write(std::uint8_t value) {
        const unsigned next_sel = value & 1, next_clr = (value >> 1) & 1;
        if (count > 1 && next_sel && next_clr && !clr)
            port = 0;
        if (clr && !next_clr && sel && next_sel)
            activate();
        else if (count > 1 && !clr && !next_clr && !sel && next_sel) {
            if (port < 5)
                ++port;
            activate();
        }
        sel = static_cast<std::uint8_t>(next_sel);
        clr = static_cast<std::uint8_t>(next_clr);
    }
    std::uint8_t read() const {
        if (clr || port >= 5)
            return 0;
        if (port >= count)
            return 15;
        if (modes[port] == 6 && phase[port])
            return sel ? 0 : static_cast<std::uint8_t>((~buttons[port] >> 8) & 15);
        return static_cast<std::uint8_t>((~buttons[port] >> (sel ? 4 : 0)) & 15);
    }
    bool valid() const {
        if (count < 1 || count > 5 || port > 5 || sel > 1 || clr > 1)
            return false;
        for (unsigned n = 0; n < 5; ++n)
            if ((modes[n] != 2 && modes[n] != 6) || phase[n] > 1 || (buttons[n] & 0xf000))
                return false;
        return true;
    }
};
} // namespace rxpce_input
