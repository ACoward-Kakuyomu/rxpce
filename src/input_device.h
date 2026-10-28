#pragma once
#include <string>
namespace rxpce_input {
inline std::wstring xinput_device_name(unsigned slot, bool connected) {
    return L"XInput " + std::to_wstring(slot + 1) +
           (connected ? L" (Connected)" : L" (Disconnected)");
}
inline std::wstring directinput_device_name(const std::wstring& name, bool connected = true) {
    return std::wstring(L"DirectInput: ") + (connected ? L"" : L"Disconnected: ") + name;
}
inline int xinput_slot(const std::string& device) {
    return device.size() == 8 && device.compare(0, 7, "xinput:") == 0 && device[7] >= '0' &&
                   device[7] <= '3'
               ? device[7] - '0'
               : -1;
}
inline bool device_enabled(const std::string& device, bool xinput, bool directinput) {
    return device == "keyboard" || (xinput_slot(device) >= 0 ? xinput : directinput);
}
} // namespace rxpce_input
