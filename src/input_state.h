#pragma once
#include <array>
#include <cstdint>

namespace rxpce_input {

enum Button : std::uint32_t {
    button_i = 1,
    button_ii = 2,
    select = 4,
    run = 8,
    up = 0x10,
    right = 0x20,
    down = 0x40,
    left = 0x80,
    button_iii = 0x100,
    button_iv = 0x200,
    button_v = 0x400,
    button_vi = 0x800
};
constexpr int axis_range = 1000;
constexpr int deadzone = 200;

struct PadSnapshot {
    bool connected = false, has_x = false, has_y = false;
    std::int32_t x = 0, y = 0;
    int axis_deadzone = deadzone;
    std::uint32_t directions = 0;
    std::array<std::uint32_t, 4> pov{{0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu}};
    std::array<std::uint8_t, 32> buttons{};
};

struct PadMapping {
    int i = 0, ii = 1, rapid_i = 4, rapid_ii = 5, run = 6, select = 7;
};

struct PadControls {
    std::uint32_t buttons = 0;
    bool rapid_i = false, rapid_ii = false;
};

inline PadControls pad_controls(const PadSnapshot& pad, const PadMapping& mapping) {
    PadControls result;
    if (!pad.connected)
        return result;
    result.buttons = pad.directions & (up | down | left | right);
    if (pad.has_x) {
        if (pad.x < -pad.axis_deadzone)
            result.buttons |= left;
        if (pad.x > pad.axis_deadzone)
            result.buttons |= right;
    }
    if (pad.has_y) {
        if (pad.y < -pad.axis_deadzone)
            result.buttons |= up;
        if (pad.y > pad.axis_deadzone)
            result.buttons |= down;
    }
    static const std::uint32_t directions[] = {up,   up | right,  right, right | down,
                                               down, down | left, left,  left | up};
    for (auto angle : pad.pov)
        if (angle < 36000)
            result.buttons |= directions[((angle + 2250) / 4500) % 8];
    const auto held = [&](int index) {
        return index >= 0 && index < static_cast<int>(pad.buttons.size()) &&
               (pad.buttons[index] & 0x80) != 0;
    };
    if (held(mapping.i))
        result.buttons |= button_i;
    if (held(mapping.ii))
        result.buttons |= button_ii;
    if (held(mapping.run))
        result.buttons |= run;
    if (held(mapping.select))
        result.buttons |= select;
    result.rapid_i = held(mapping.rapid_i);
    result.rapid_ii = held(mapping.rapid_ii);
    return result;
}

class TurboButton {
    unsigned phase_ = 0;

  public:
    bool sample(bool normal, bool rapid) {
        if (!rapid) {
            phase_ = 0;
            return normal;
        }
        const bool on = phase_ == 0;
        phase_ = (phase_ + 1) % 3;
        return on;
    }
};

class InputProcessor {
    TurboButton i_, ii_;

  public:
    void reset() {
        i_ = TurboButton{};
        ii_ = TurboButton{};
    }
    std::uint32_t process(std::uint32_t buttons, bool rapid_i, bool rapid_ii) {
        if ((buttons & (up | down)) == (up | down))
            buttons &= ~(up | down);
        if ((buttons & (left | right)) == (left | right))
            buttons &= ~(left | right);
        const bool first = i_.sample((buttons & button_i) != 0, rapid_i);
        const bool second = ii_.sample((buttons & button_ii) != 0, rapid_ii);
        buttons &= ~(button_i | button_ii);
        if (first)
            buttons |= button_i;
        if (second)
            buttons |= button_ii;
        return buttons;
    }
};

} // namespace rxpce_input
