#pragma once
#include <windows.h>
#include <Xinput.h>
#include "input_state.h"
#include "input_device.h"
namespace rxpce_input {
inline const wchar_t* xinput_button_name(unsigned button) {
    static const wchar_t* names[] = {L"A",          L"B",           L"X",    L"Y",
                                     L"LB",         L"RB",          L"Back", L"Start",
                                     L"Left stick", L"Right stick", L"LT",   L"RT"};
    return button < 12 ? names[button] : L"";
}
inline PadSnapshot convert_xinput(const XINPUT_GAMEPAD& native) {
    PadSnapshot result;
    result.connected = true;
    result.has_x = result.has_y = true;
    result.x = native.sThumbLX;
    result.y = -static_cast<std::int32_t>(native.sThumbLY);
    result.axis_deadzone = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
    if (native.wButtons & XINPUT_GAMEPAD_DPAD_UP)
        result.directions |= up;
    if (native.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
        result.directions |= down;
    if (native.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
        result.directions |= left;
    if (native.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
        result.directions |= right;
    const WORD bits[] = {XINPUT_GAMEPAD_A,
                         XINPUT_GAMEPAD_B,
                         XINPUT_GAMEPAD_X,
                         XINPUT_GAMEPAD_Y,
                         XINPUT_GAMEPAD_LEFT_SHOULDER,
                         XINPUT_GAMEPAD_RIGHT_SHOULDER,
                         XINPUT_GAMEPAD_BACK,
                         XINPUT_GAMEPAD_START,
                         XINPUT_GAMEPAD_LEFT_THUMB,
                         XINPUT_GAMEPAD_RIGHT_THUMB};
    for (unsigned n = 0; n < 10; ++n)
        if (native.wButtons & bits[n])
            result.buttons[n] = 0x80;
    if (native.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
        result.buttons[10] = 0x80;
    if (native.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
        result.buttons[11] = 0x80;
    return result;
}
using XInputReader = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
inline bool read_xinput(unsigned slot, PadSnapshot& result, bool active,
                        XInputReader read = XInputGetState) {
    result = PadSnapshot{};
    if (slot >= XUSER_MAX_COUNT || !active)
        return false;
    XINPUT_STATE native{};
    if (read(slot, &native) != ERROR_SUCCESS)
        return false;
    result = convert_xinput(native.Gamepad);
    return true;
}
class XInputPoller {
    std::array<std::uint64_t, 4> retry_at{};

  public:
    void reset() {
        retry_at.fill(0);
    }
    bool read(unsigned slot, PadSnapshot& result, bool active, std::uint64_t now = GetTickCount64(),
              XInputReader api = XInputGetState) {
        result = PadSnapshot{};
        if (slot >= retry_at.size() || !active || now < retry_at[slot])
            return false;
        const bool ok = read_xinput(slot, result, true, api);
        retry_at[slot] = ok ? 0 : now + 2000;
        return ok;
    }
};
} // namespace rxpce_input
