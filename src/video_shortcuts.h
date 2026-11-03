#pragma once
#include <windows.h>

namespace rxpce_video {
enum class ShortcutCommand {
    none,
    zoom,
    fullscreen,
    skip_down,
    skip_up,
    toggle_auto,
    save,
    load,
    toggle_fps,
    stop,
    quit,
    reset,
    input_profile,
    record_replay,
    play_replay
};
struct ShortcutResult {
    bool handled;
    ShortcutCommand command;
    int profile_slot = 0;
};
class Shortcuts {
  public:
    void reset() {
        alt_shortcut = false;
        profile_digits = 0;
    }
    ShortcutResult process(UINT message, WPARAM key, LPARAM flags, bool running,
                           bool fullscreen = false) {
        const bool repeated = (flags & (LPARAM(1) << 30)) != 0;
        const bool alt = (flags & (LPARAM(1) << 29)) != 0;
        if ((key == VK_F11 || key == VK_F12) &&
            (message == WM_KEYDOWN || message == WM_KEYUP || message == WM_SYSKEYDOWN ||
             message == WM_SYSKEYUP)) {
            if (message != WM_KEYDOWN || repeated || alt)
                return {true, ShortcutCommand::none};
            return {true, key == VK_F12 ? ShortcutCommand::play_replay
                                        : running ? ShortcutCommand::record_replay
                                                  : ShortcutCommand::none};
        }
        if (key >= '0' && key <= '9') {
            const unsigned digit = static_cast<unsigned>(key - '0');
            const unsigned bit = 1u << digit;
            if (message == WM_SYSKEYDOWN && alt) {
                alt_shortcut = true;
                profile_digits |= bit;
                return {true, ShortcutCommand::input_profile,
                        running || repeated ? 0 : (digit == 0 ? 10 : static_cast<int>(digit))};
            }
            if ((message == WM_SYSKEYUP || message == WM_KEYUP) && (profile_digits & bit)) {
                profile_digits &= ~bit;
                return {true, ShortcutCommand::input_profile};
            }
            if (message == WM_KEYDOWN && repeated && (profile_digits & bit))
                return {true, ShortcutCommand::input_profile};
        }
        if (key == VK_F10 && (message == WM_KEYDOWN || message == WM_KEYUP ||
                              message == WM_SYSKEYDOWN || message == WM_SYSKEYUP)) {
            const bool pressed = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
            return {true, pressed && running && !repeated && !alt ? ShortcutCommand::reset
                                                                  : ShortcutCommand::none};
        }
        if (key == VK_ESCAPE && (message == WM_KEYDOWN || message == WM_KEYUP))
            return {true, message == WM_KEYDOWN && running && !repeated ? ShortcutCommand::stop
                                                                        : ShortcutCommand::none};
        if (key == VK_F8 && (message == WM_KEYDOWN || message == WM_KEYUP))
            return {true, message == WM_KEYDOWN && !repeated && !fullscreen
                              ? ShortcutCommand::zoom
                              : ShortcutCommand::none};
        if ((message == WM_KEYDOWN || message == WM_KEYUP) && key >= VK_F2 && key <= VK_F7) {
            if (message == WM_KEYUP || !running || repeated)
                return {true, ShortcutCommand::none};
            if (key == VK_F5)
                return {true, ShortcutCommand::save};
            if (key == VK_F6)
                return {true, ShortcutCommand::load};
            if (key == VK_F7)
                return {true, ShortcutCommand::toggle_fps};
            return {true, key == VK_F2   ? ShortcutCommand::toggle_auto
                          : key == VK_F3 ? ShortcutCommand::skip_down
                                         : ShortcutCommand::skip_up};
        }
        if (message == WM_SYSKEYDOWN && key == VK_F4 && (flags & (LPARAM(1) << 29)))
            return {true, repeated ? ShortcutCommand::none : ShortcutCommand::quit};
        if (message == WM_SYSKEYDOWN && key == VK_RETURN && (flags & (LPARAM(1) << 29))) {
            alt_shortcut = true;
            return {true,
                    running && !repeated ? ShortcutCommand::fullscreen : ShortcutCommand::none};
        }
        if (key == VK_RETURN && (message == WM_SYSCHAR || message == WM_SYSKEYUP))
            return {true, ShortcutCommand::none};
        if ((message == WM_SYSKEYUP || message == WM_KEYUP) && key == VK_MENU && alt_shortcut) {
            alt_shortcut = false;
            return {true, ShortcutCommand::none};
        }
        return {false, ShortcutCommand::none};
    }

  private:
    bool alt_shortcut = false;
    unsigned profile_digits = 0;
};
} // namespace rxpce_video
