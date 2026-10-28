#pragma once
#include "settings.h"
#include "input_state.h"
#include "input_device.h"
#include <vector>
namespace rxpce_input {
inline std::vector<int> mapping_actions(bool six, bool keyboard) {
    if (six)
        return keyboard ? std::vector<int>{0, 1, 10, 11, 12, 13, 4, 5, 6, 7, 8, 9}
                        : std::vector<int>{0, 1, 10, 11, 12, 13, 4, 5};
    return keyboard ? std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}
                    : std::vector<int>{0, 1, 2, 3, 4, 5};
}
inline int pad_binding(int action) {
    return action < 6 ? action : action >= 10 ? action - 4 : -1;
}
class ControllerInput {
    InputProcessor processor;

  public:
    void reset() {
        processor.reset();
    }
    std::uint16_t sample(const rxpce_config::Settings::Player& config, const unsigned char* keys,
                         const PadSnapshot& pad) {
        std::uint32_t buttons = 0;
        bool rapid_i = false, rapid_ii = false;
        const bool six = config.buttons == 6;
        if (config.device == "keyboard") {
            const auto held = [&](unsigned n) {
                const int k = config.keyboard[n];
                return k > 0 && k < 256 && (keys[k] & 0x80) != 0;
            };
            const std::uint32_t bit[] = {button_i,   button_ii, 0,        0,        run,
                                         select,     down,      up,       left,     right,
                                         button_iii, button_iv, button_v, button_vi};
            for (unsigned n = 0; n < (six ? 14u : 10u); ++n)
                if (held(n))
                    buttons |= bit[n];
            rapid_i = !six && held(2);
            rapid_ii = !six && held(3);
            if ((keys[0x12] & 0x80) && (keys[0x0d] & 0x80))
                buttons &= ~run;
        } else {
            const auto& mapping = xinput_slot(config.device) >= 0 ? config.xinput : config.pad;
            const auto controls = pad_controls(pad, PadMapping{mapping[0], mapping[1], mapping[2],
                                                               mapping[3], mapping[4], mapping[5]});
            buttons = controls.buttons;
            rapid_i = !six && controls.rapid_i;
            rapid_ii = !six && controls.rapid_ii;
            if (six && pad.connected)
                for (unsigned n = 0; n < 4; ++n) {
                    const int key = mapping[n + 6];
                    if (key >= 0 && key < 32 && (pad.buttons[key] & 0x80))
                        buttons |= (button_iii << n);
                }
        }
        return static_cast<std::uint16_t>(processor.process(buttons, rapid_i, rapid_ii));
    }
};
} // namespace rxpce_input
