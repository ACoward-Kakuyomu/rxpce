#include "host_input.h"
#include "machine.h"
#include "controller_input.h"
#include "xinput_state.h"
#include "keyboard_input.h"
#include "settings.h"
#include "di.h"
#include "video.h"
#include "log.h"
#include <algorithm>

namespace rxpce_host {
namespace {
std::array<rxpce_input::ControllerInput, 5> processors;
rxpce_input::XInputPoller xinput;
std::array<std::uint16_t, 5> previous_buttons{};
bool previous_blocked = false, previous_background = false, previous_sprites = false;
unsigned log_changes = 0;
} // namespace
void ResetInput() {
    for (auto& processor : processors)
        processor.reset();
}
void InitializeInput() {
    xinput.reset();
    ResetInput();
    previous_buttons.fill(0);
    previous_blocked = previous_background = previous_sprites = false;
    log_changes = 0;
    const auto& config = rxpce_config::active_input;
    LogDumpDetail("Input configured: players=%d\n", config.count);
    for (int n = 0; n < config.count; ++n) {
        const auto& player = config.players[n];
        const auto device =
            rxpce_input::xinput_slot(player.device) >= 0 || player.device == "keyboard"
                ? player.device.c_str()
                : "DirectInput";
        LogDumpDetail("Input player %d device=%s buttons=%d\n", n + 1, device, player.buttons);
    }
}
void PollInput(rxpce_machine::Machine& machine) {
    const auto& config = rxpce_config::active_input;
    std::array<unsigned char, 256> keys{};
    rxpce_input::keyboard_state().snapshot(keys.data());
    bool pad_down = false;
    std::array<rxpce_input::PadSnapshot, 5> pads;
    for (int n = 0; n < config.count; ++n) {
        const auto& player = config.players[n];
        if (!rxpce_input::device_enabled(player.device, config.xinput_enabled,
                                         config.directinput_enabled))
            continue;
        const auto slot = rxpce_input::xinput_slot(player.device);
        if (slot >= 0)
            xinput.read(slot, pads[n], bActiveApp);
        else if (player.device != "keyboard")
            ReadJoystick(n, pads[n]);
        for (auto button : pads[n].buttons)
            if (button & 0x80)
                pad_down = true;
    }
    const bool blocked = rxpce_config::block_game_input(keys.data(), pad_down);
    if (blocked != previous_blocked) {
        LogDumpDetail("Input blocked=%u keyboard_held=%u pad_held=%u\n", blocked,
                      std::any_of(keys.begin(), keys.end(),
                                  [](unsigned char key) { return (key & 0x80) != 0; }),
                      pad_down);
        previous_blocked = blocked;
    }
    if (blocked) {
        keys.fill(0);
        pads = {};
        for (auto& processor : processors)
            processor.reset();
    }
    auto& buttons = machine.io.controllers.buttons;
    buttons.fill(0);
    for (unsigned n = 0; n < static_cast<unsigned>(config.count); ++n) {
        const auto& player = config.players[n];
        if (rxpce_input::device_enabled(player.device, config.xinput_enabled,
                                        config.directinput_enabled))
            buttons[n] = processors[n].sample(player, keys.data(), pads[n]);
    }
    for (unsigned n = 0; n < buttons.size(); ++n)
        if (previous_buttons[n] != buttons[n]) {
            if (log_changes++ < 32)
                LogDumpDetail("Input player %u buttons=%03x\n", n + 1, buttons[n]);
            previous_buttons[n] = buttons[n];
        }
    const bool background = (keys['1'] & 0x80) != 0, sprites = (keys['2'] & 0x80) != 0;
    if (background && !previous_background)
        machine.video.background_enabled = !machine.video.background_enabled;
    if (sprites && !previous_sprites)
        machine.video.sprites_enabled = !machine.video.sprites_enabled;
    previous_background = background;
    previous_sprites = sprites;
}
} // namespace rxpce_host
