// DirectInput 8 devices for the configured controller slots.
#include "win32_support.h"
#ifdef USE_JOYSTICK
#include <dinput.h>
#include "di.h"
#include "video.h"
#include "settings.h"
#include "input_device.h"
#include "log.h"
#include "callback_boundary.h"
#include <algorithm>
#include <array>
#include <vector>
#include <wrl/client.h>
namespace {
Microsoft::WRL::ComPtr<IDirectInput8> direct_input;
struct PadDevice {
    Microsoft::WRL::ComPtr<IDirectInputDevice8> device;
    bool x = false, y = false;
    HRESULT status = E_PENDING;
};
std::array<PadDevice, 5> pads;
struct DeviceCollection {
    std::vector<GUID> ids;
    rxpce_host::CallbackFailure failure;
};
BOOL CALLBACK collect_device(const DIDEVICEINSTANCE* instance, void* context) {
    auto& collection = *static_cast<DeviceCollection*>(context);
    return collection.failure.invoke([&] { collection.ids.push_back(instance->guidInstance); })
               ? DIENUM_CONTINUE
               : DIENUM_STOP;
}
void report_status(unsigned player, HRESULT status) {
    auto& pad = pads[player];
    if (pad.status != status)
        LogDumpDetail("DirectInput player %u poll/acquire=%08lx\n", player + 1, status);
    pad.status = status;
}
bool configure_axis(IDirectInputDevice8* device, DWORD offset) {
    DIPROPRANGE range{};
    range.diph.dwSize = sizeof(range);
    range.diph.dwHeaderSize = sizeof(range.diph);
    range.diph.dwObj = offset;
    range.diph.dwHow = DIPH_BYOFFSET;
    range.lMin = -rxpce_input::axis_range;
    range.lMax = rxpce_input::axis_range;
    if (FAILED(device->SetProperty(DIPROP_RANGE, &range.diph)))
        return false;
    DIPROPDWORD deadzone{};
    deadzone.diph = range.diph;
    deadzone.diph.dwSize = sizeof(deadzone);
    deadzone.dwData = 0;
    device->SetProperty(DIPROP_DEADZONE, &deadzone.diph);
    return true;
}
} // namespace
BOOL InitDInput() {
    ReleaseDInput();
    const auto& config = rxpce_config::active_input;
    if (!config.directinput_enabled)
        return FALSE;
    bool needed = false;
    for (int n = 0; n < config.count; ++n)
        needed |= config.players[n].device != "keyboard" &&
                  rxpce_input::xinput_slot(config.players[n].device) < 0;
    if (!needed)
        return FALSE;
    auto status = DirectInput8Create(
        GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8,
        reinterpret_cast<void**>(direct_input.ReleaseAndGetAddressOf()), nullptr);
    LogDumpDetail("DirectInput interface=%08lx\n", status);
    if (FAILED(status))
        return FALSE;
    DeviceCollection collection;
    auto& available = collection.ids;
    status = direct_input->EnumDevices(DI8DEVCLASS_GAMECTRL, collect_device, &collection,
                                       DIEDFL_ATTACHEDONLY);
    LogDumpDetail("DirectInput enumerate=%08lx attached=%u\n", status,
                  static_cast<unsigned>(available.size()));
    if (FAILED(status) || collection.failure.failed()) {
        LogDump("DirectInput device collection failed\n");
        ReleaseDInput();
        return FALSE;
    }
    bool connected = false;
    for (int n = 0; n < config.count; ++n) {
        if (config.players[n].device == "keyboard" ||
            rxpce_input::xinput_slot(config.players[n].device) >= 0)
            continue;
        GUID id{};
        const auto guid = rxpce_config::to_wide(config.players[n].device);
        auto& pad = pads[n];
        if (FAILED(CLSIDFromString(guid.c_str(), &id)))
            continue;
        const auto found = std::find_if(available.begin(), available.end(), [&](const GUID& item) {
            return IsEqualGUID(item, id) != FALSE;
        });
        if (found == available.end()) {
            LogDumpDetail("DirectInput player %d disconnected\n", n + 1);
            continue;
        }
        status = direct_input->CreateDevice(*found, pad.device.ReleaseAndGetAddressOf(), nullptr);
        LogDumpDetail("DirectInput player %d create=%08lx\n", n + 1, status);
        if (FAILED(status))
            continue;
        status = pad.device->SetDataFormat(&c_dfDIJoystick);
        LogDumpDetail("DirectInput player %d format=%08lx\n", n + 1, status);
        if (SUCCEEDED(status)) {
            status = pad.device->SetCooperativeLevel(hwnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
            LogDumpDetail("DirectInput player %d cooperative=%08lx\n", n + 1, status);
        }
        if (FAILED(status)) {
            pad.device.Reset();
            continue;
        }
        pad.x = configure_axis(pad.device.Get(), DIJOFS_X);
        pad.y = configure_axis(pad.device.Get(), DIJOFS_Y);
        report_status(n, pad.device->Acquire());
        connected = true;
    }
    return connected;
}
BOOL ReadJoystick(unsigned player, rxpce_input::PadSnapshot& state) {
    state = rxpce_input::PadSnapshot{};
    if (player >= pads.size() || !rxpce_config::active_input.directinput_enabled)
        return FALSE;
    if (!bActiveApp) {
        if (pads[player].device)
            report_status(player, S_FALSE);
        return FALSE;
    }
    const auto& pad = pads[player];
    if (!pad.device)
        return FALSE;
    DIJOYSTATE native{};
    std::fill_n(native.rgdwPOV, 4, 0xfffffffful);
    auto poll = [&] {
        auto result = pad.device->Poll();
        return FAILED(result) ? result : pad.device->GetDeviceState(sizeof(native), &native);
    };
    auto result = poll();
    if (result == DIERR_INPUTLOST || result == DIERR_NOTACQUIRED) {
        const auto acquired = pad.device->Acquire();
        if (FAILED(acquired)) {
            report_status(player, acquired);
            return FALSE;
        }
        result = poll();
    }
    report_status(player, result);
    if (FAILED(result))
        return FALSE;
    state.connected = true;
    state.has_x = pad.x;
    state.has_y = pad.y;
    state.x = native.lX;
    state.y = native.lY;
    std::copy_n(native.rgdwPOV, 4, state.pov.begin());
    std::copy_n(native.rgbButtons, 32, state.buttons.begin());
    return TRUE;
}
BOOL ReleaseDevices() {
    for (auto& pad : pads) {
        if (pad.device)
            pad.device->Unacquire();
        pad = PadDevice{};
    }
    return TRUE;
}
BOOL ReleaseDInput() {
    ReleaseDevices();
    direct_input.Reset();
    return TRUE;
}
#endif
