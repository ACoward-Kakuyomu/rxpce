#define NOMINMAX
#include "settings_dialog.h"
#include "controller_input.h"
#include "xinput_state.h"
#include <dxgi.h>
#include <wrl/client.h>
#include <algorithm>
#include <stdexcept>
#include <new>
#include <cwchar>
#include <shobjidl.h>
#include "cd_session.h"

using namespace rxpce_config;
namespace {
enum {
    adapter = 2100,
    zoom,
    auto_skip,
    skip,
    skip_slider,
    palette,
    palette_browse,
    sound = 2120,
    rate,
    channels,
    latency,
    latency_slider,
    coop,
    coop_label,
    ds_backend = 2130,
    wasapi_backend,
    device = 2140,
    count,
    port,
    mode,
    map_base = 2150,
    label_base = 2170,
    rom = 2190,
    rom_browse,
    state,
    state_browse,
    bios,
    bios_browse,
    bios_info,
    clock_edit = 2200,
    log_enabled,
    fps,
    xinput_support,
    directinput_support
};
std::wstring key_name(int key) {
    wchar_t name[128]{};
    UINT scan = MapVirtualKeyW(key, MAPVK_VK_TO_VSC);
    if (key >= VK_PRIOR && key <= VK_DOWN)
        scan |= 0x100;
    if (GetKeyNameTextW(static_cast<LONG>(scan << 16), name, 128))
        return name;
    return L"VK " + std::to_wstring(key);
}
void bios_description(HWND owner, const std::string& path) {
    std::wstring text = L"Checked when CD emulation starts";
    if (path.empty())
        text = L"No System Card selected";
    else
        try {
            rxpce_cd::SystemCard card;
            std::string error;
            if (rxpce_cd::load_system_card(rxpce_cd::bios_path(to_wide(path)), card, error)) {
                const wchar_t* versions[] = {L"?", L"1.0", L"2.0", L"2.1", L"3.0"};
                text = L"Japanese System Card " +
                       std::wstring(versions[static_cast<unsigned>(card.profile.version)]);
            } else
                text = L"Unavailable or unsupported BIOS";
        } catch (...) {
            text = L"Unavailable or unsupported BIOS";
        }
    ::SetWindowTextW(::GetDlgItem(owner, bios_info), text.c_str());
}
} // namespace
HWND SettingsDialog::add(int page, const wchar_t* cls, const wchar_t* title, int id, int x, int y,
                         int width, int height, DWORD style, DWORD ex) {
    RECT bounds{x, y, x + width, y + height};
    MapDialogRect(&bounds);
    HWND control = CreateWindowExW(
        ex, cls, title, WS_CHILD | WS_VISIBLE | style, bounds.left, bounds.top,
        bounds.right - bounds.left, bounds.bottom - bounds.top, m_hWnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), _Module.GetModuleInstance(), nullptr);
    if (!control)
        throw std::runtime_error("Cannot create settings control");
    ::SendMessage(control, WM_SETFONT, SendMessage(WM_GETFONT), TRUE);
    pages[page].push_back(control);
    return control;
}
void SettingsDialog::label(int page, const wchar_t* title, int x, int y, int width) {
    add(page, L"STATIC", title, 0, x, y, width, 12);
}
void SettingsDialog::combo_item(int id, const std::wstring& text, LPARAM data) {
    HWND control = GetDlgItem(id);
    const LRESULT index =
        ::SendMessageW(control, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
    ::SendMessage(control, CB_SETITEMDATA, index, data);
}
void SettingsDialog::select_data(int id, LPARAM data) {
    HWND control = GetDlgItem(id);
    for (int i = 0; i < ::SendMessage(control, CB_GETCOUNT, 0, 0); ++i)
        if (::SendMessage(control, CB_GETITEMDATA, i, 0) == data) {
            ::SendMessage(control, CB_SETCURSEL, i, 0);
            return;
        }
}
int SettingsDialog::selected(int id) {
    HWND control = GetDlgItem(id);
    const auto index = ::SendMessage(control, CB_GETCURSEL, 0, 0);
    if (index == CB_ERR)
        throw std::runtime_error("Select an item from each list.");
    return static_cast<int>(::SendMessage(control, CB_GETITEMDATA, index, 0));
}
int SettingsDialog::number(int id) {
    wchar_t buffer[64]{};
    ::GetWindowTextW(GetDlgItem(id), buffer, 64);
    wchar_t* end = nullptr;
    const long long n = wcstoll(buffer, &end, 10);
    if (!buffer[0] || *end || n < 0 || n > 2147483647LL)
        throw std::runtime_error("Enter a valid whole number.");
    return static_cast<int>(n);
}
void SettingsDialog::set_number(int id, int n) {
    ::SetWindowTextW(GetDlgItem(id), std::to_wstring(n).c_str());
}
void SettingsDialog::set_text(int id, const std::string& utf8) {
    ::SetWindowTextW(GetDlgItem(id), to_wide(utf8).c_str());
}
std::string SettingsDialog::text(int id) {
    HWND control = GetDlgItem(id);
    std::wstring buffer(::GetWindowTextLengthW(control) + 1, L'\0');
    const int count = ::GetWindowTextW(control, &buffer[0], static_cast<int>(buffer.size()));
    buffer.resize(count);
    return to_utf8(buffer);
}
void SettingsDialog::slider(int page, int id, int x, int y, int width, int low, int high,
                            int current) {
    HWND control = add(page, TRACKBAR_CLASSW, L"", id, x, y, width, 18, WS_TABSTOP | TBS_AUTOTICKS);
    ::SendMessage(control, TBM_SETRANGE, TRUE, MAKELPARAM(low, high));
    ::SendMessage(control, TBM_SETPOS, TRUE, current);
}
void SettingsDialog::show_page(int page) {
    for (int p = 0; p < 5; ++p)
        for (HWND control : pages[p])
            ::ShowWindow(control, p == page ? SW_SHOW : SW_HIDE);
    if (page == 2)
        for (int i = static_cast<int>(actions.size()); i < 12; ++i) {
            ::ShowWindow(GetDlgItem(map_base + i), SW_HIDE);
            ::ShowWindow(GetDlgItem(label_base + i), SW_HIDE);
        }
}
BOOL CALLBACK SettingsDialog::enum_pad(const DIDEVICEINSTANCEA* instance, void* context) {
    auto& dialog = *static_cast<SettingsDialog*>(context);
    return dialog.enumeration_failure.invoke([&] {
        wchar_t guid[40]{};
        if (StringFromGUID2(instance->guidInstance, guid, 40) == 0)
            return;
        dialog.devices.push_back({to_utf8(guid), rxpce_input::directinput_device_name(to_wide(
                                                     from_native(instance->tszInstanceName)))});
    })
               ? DIENUM_CONTINUE
               : DIENUM_STOP;
}
LRESULT SettingsDialog::abort_dialog(const char* error) noexcept {
    KillTimer(1);
    ::MessageBoxA(m_hWnd, error, "Settings error", MB_OK | MB_ICONERROR);
    EndDialog(IDCANCEL);
    return 0;
}
LRESULT SettingsDialog::OnInit(UINT, WPARAM, LPARAM, BOOL&) {
    try {

        CenterWindow(GetParent());
        const wchar_t* titles[] = {L"Video", L"Audio", L"Input", L"Folders", L"Advanced"};
        for (int i = 0; i < 5; ++i) {
            TCITEMW item{};
            item.mask = TCIF_TEXT;
            item.pszText = const_cast<wchar_t*>(titles[i]);
            ::SendMessageW(GetDlgItem(2000), TCM_INSERTITEMW, i, reinterpret_cast<LPARAM>(&item));
        }
        const DWORD combo = WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL;
        const DWORD edit = WS_TABSTOP | ES_AUTOHSCROLL;
        label(0, L"D3D11 device", 20, 32);
        add(0, L"COMBOBOX", L"", adapter, 130, 30, 260, 130, combo);
        combo_item(adapter, L"Automatic (hardware / WARP fallback)", -1);
        combo_item(adapter, L"WARP (software)", -2);
        Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
        if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
            for (UINT index = 0;; ++index) {
                Microsoft::WRL::ComPtr<IDXGIAdapter1> gpu;
                if (factory->EnumAdapters1(index, &gpu) == DXGI_ERROR_NOT_FOUND)
                    break;
                if (!gpu)
                    break;
                DXGI_ADAPTER_DESC1 desc{};
                gpu->GetDesc1(&desc);
                combo_item(adapter, desc.Description, index);
            }
        select_data(adapter, value.video.adapter);
        if (::SendMessage(GetDlgItem(adapter), CB_GETCURSEL, 0, 0) == CB_ERR) {
            combo_item(adapter, L"Unavailable adapter " + std::to_wstring(value.video.adapter),
                       value.video.adapter);
            select_data(adapter, value.video.adapter);
        }
        label(0, L"Window scale", 20, 54);
        add(0, L"COMBOBOX", L"", zoom, 130, 52, 72, 100, combo);
        for (int n = 1; n <= 4; ++n)
            combo_item(zoom, std::to_wstring(n) + L"x", n);
        select_data(zoom, value.video.zoom);
        add(0, L"BUTTON", L"Automatic frame skip (60 Hz)", auto_skip, 20, 76, 300, 14,
            WS_TABSTOP | BS_AUTOCHECKBOX);
        CheckDlgButton(auto_skip, value.video.auto_frame_skip);
        label(0, L"Frame skip (0-15)", 20, 98);
        add(0, L"EDIT", L"", skip, 130, 96, 45, 14, edit | ES_NUMBER, WS_EX_CLIENTEDGE);
        set_number(skip, value.video.frame_skip);
        slider(0, skip_slider, 180, 94, 210, 0, 15, value.video.frame_skip);
        label(0, L"Palette file (empty = linear RGB)", 20, 122, 340);
        add(0, L"EDIT", L"", palette, 20, 136, 330, 14, edit, WS_EX_CLIENTEDGE);
        set_text(palette, value.video.palette_file);
        add(0, L"BUTTON", L"...", palette_browse, 358, 136, 32, 14, WS_TABSTOP);
        add(1, L"BUTTON", L"Enable audio", sound, 20, 30, 100, 14, WS_TABSTOP | BS_AUTOCHECKBOX);
        CheckDlgButton(sound, value.audio.enabled);
        add(1, L"BUTTON", L"DirectSound", ds_backend, 130, 30, 100, 14,
            WS_GROUP | WS_TABSTOP | BS_AUTORADIOBUTTON);
        add(1, L"BUTTON", L"WASAPI (shared)", wasapi_backend, 245, 30, 145, 14,
            WS_TABSTOP | BS_AUTORADIOBUTTON);
        CheckRadioButton(ds_backend, wasapi_backend,
                         value.audio.backend == "wasapi" ? wasapi_backend : ds_backend);
        label(1, L"Sample rate (Hz)", 20, 56);
        add(1, L"COMBOBOX", L"", rate, 130, 54, 110, 150, combo);
        for (int n : {48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000})
            combo_item(rate, std::to_wstring(n), n);
        select_data(rate, value.audio.sample_rate);
        label(1, L"Channels", 20, 78);
        add(1, L"COMBOBOX", L"", channels, 130, 76, 110, 60, combo);
        combo_item(channels, L"Mono", 1);
        combo_item(channels, L"Stereo", 2);
        select_data(channels, value.audio.channels);
        label(1, L"Latency (20-160 ms)", 20, 100, 110);
        add(1, L"EDIT", L"", latency, 130, 98, 45, 14, edit | ES_NUMBER, WS_EX_CLIENTEDGE);
        set_number(latency, value.audio.latency_ms);
        slider(1, latency_slider, 180, 96, 210, 20, 160, value.audio.latency_ms);
        label(1, L"DirectSound level", 20, 122);
        slider(1, coop, 130, 118, 260, 0, 2, value.audio.cooperative_level);
        add(1, L"STATIC", L"0: Write primary   1: Exclusive   2: Priority", coop_label, 20, 138,
            370, 12);
        update_audio_controls();
        label(2, L"Players", 20, 30, 45);
        add(2, L"COMBOBOX", L"", count, 65, 28, 45, 100, combo);
        for (int n = 1; n <= 5; ++n)
            combo_item(count, std::to_wstring(n), n);
        select_data(count, value.input.count);
        label(2, L"Player", 130, 30, 40);
        add(2, L"COMBOBOX", L"", port, 175, 28, 45, 100, combo);
        list_players();
        label(2, L"Buttons", 245, 30, 45);
        add(2, L"COMBOBOX", L"", mode, 300, 28, 90, 70, combo);
        combo_item(mode, L"2 buttons", 2);
        combo_item(mode, L"6 buttons", 6);
        label(2, L"Input device", 20, 50);
        add(2, L"COMBOBOX", L"", device, 130, 48, 260, 150, combo);
        for (int n = 0; n < 12; ++n) {
            const int x = n < 6 ? 20 : 220, y = 72 + (n % 6) * 13;
            add(2, L"STATIC", L"", label_base + n, x, y, 45, 12);
            HWND control = add(2, L"COMBOBOX", L"", map_base + n, x + 45, y - 2, n < 6 ? 135 : 125,
                               140, combo);
            if (!mappings[n].SubclassWindow(control))
                throw std::runtime_error("Cannot attach input binding control");
        }
        load_player();
        if (!SetTimer(1, 50))
            throw std::runtime_error("Cannot start input preview timer");
        label(3, L"ROM folder", 20, 32);
        add(3, L"EDIT", L"", rom, 20, 48, 330, 14, edit, WS_EX_CLIENTEDGE);
        set_text(rom, value.folders.rom);
        add(3, L"BUTTON", L"...", rom_browse, 358, 48, 32, 14, WS_TABSTOP);
        label(3, L"State / RAM folder (. = ROM / ZIP / CUE folder)", 20, 68, 370);
        add(3, L"EDIT", L"", state, 20, 82, 330, 14, edit, WS_EX_CLIENTEDGE);
        set_text(state, value.folders.state);
        add(3, L"BUTTON", L"...", state_browse, 358, 82, 32, 14, WS_TABSTOP);
        label(3, L"System Card BIOS (CD only)", 20, 102, 370);
        add(3, L"EDIT", L"", bios, 20, 116, 330, 14, edit, WS_EX_CLIENTEDGE);
        set_text(bios, value.cd.bios_file);
        add(3, L"BUTTON", L"...", bios_browse, 358, 116, 32, 14, WS_TABSTOP);
        add(3, L"STATIC", L"", bios_info, 20, 134, 370, 12, 0);
        bios_description(m_hWnd, value.cd.bios_file);
        label(4, L"Base clock (Hz)", 20, 32);
        add(4, L"EDIT", L"", clock_edit, 130, 30, 130, 14, edit | ES_NUMBER, WS_EX_CLIENTEDGE);
        set_number(clock_edit, value.advanced.base_clock_hz);
        label(4, L"1,000,000 - 85,920,000 Hz; rounded down to 1,000 Hz.", 20, 52, 375);
        add(4, L"BUTTON", L"Enable diagnostic log", log_enabled, 20, 76, 300, 14,
            WS_TABSTOP | BS_AUTOCHECKBOX);
        CheckDlgButton(log_enabled, value.advanced.log);
        add(4, L"BUTTON", L"Show FPS", fps, 20, 96, 300, 14, WS_TABSTOP | BS_AUTOCHECKBOX);
        CheckDlgButton(fps, value.advanced.show_fps);
        add(4, L"BUTTON", L"XInput support", xinput_support, 20, 116, 300, 14,
            WS_TABSTOP | BS_AUTOCHECKBOX);
        CheckDlgButton(xinput_support, value.input.xinput_enabled);
        add(4, L"BUTTON", L"DirectInput support", directinput_support, 20, 136, 300, 14,
            WS_TABSTOP | BS_AUTOCHECKBOX);
        CheckDlgButton(directinput_support, value.input.directinput_enabled);
        initializing = false;
        show_page(0);
        return TRUE;
    } catch (const std::exception& error) {
        return abort_dialog(error.what());
    } catch (...) {
        return abort_dialog("Unexpected settings failure");
    }
}
void SettingsDialog::collect_mappings() {
    auto& profile = value.input.players[player];
    for (int n = 0; n < static_cast<int>(actions.size()); ++n) {
        if (mappings[n].GetCurSel() < 0)
            continue;
        const int item = static_cast<int>(mappings[n].GetItemData(mappings[n].GetCurSel()));
        if (keyboard)
            profile.keyboard[actions[n]] = item;
        else
            (xinput_player >= 0 ? profile.xinput
                                : profile.pad)[rxpce_input::pad_binding(actions[n])] = item;
    }
}
void SettingsDialog::update_audio_controls() {
    const BOOL enabled = IsDlgButtonChecked(ds_backend) != 0;
    ::EnableWindow(GetDlgItem(coop), enabled);
    ::EnableWindow(GetDlgItem(coop_label), enabled);
}
void SettingsDialog::list_players() {
    ::SendMessage(GetDlgItem(port), CB_RESETCONTENT, 0, 0);
    for (int n = 0; n < value.input.count; ++n)
        combo_item(port, std::to_wstring(n + 1), n);
    select_data(port, player);
}
void SettingsDialog::load_player() {
    rebuild_devices();
    const auto& profile = value.input.players[player];
    for (std::size_t n = 0; n < devices.size(); ++n)
        if (_stricmp(devices[n].id.c_str(), profile.device.c_str()) == 0)
            select_data(device, n);
    select_data(mode, profile.buttons);
    update_device();
}
void SettingsDialog::rebuild_devices() {
    close_preview();
    direct_input.Reset();
    devices.clear();
    devices.push_back({"keyboard", L"Keyboard"});
    if (value.input.xinput_enabled)
        for (int n = 0; n < 4; ++n)
            devices.push_back(
                {"xinput:" + std::to_string(n), rxpce_input::xinput_device_name(n, false)});
    if (value.input.directinput_enabled) {
        DirectInput8Create(_Module.GetModuleInstance(), DIRECTINPUT_VERSION, IID_IDirectInput8A,
                           reinterpret_cast<void**>(direct_input.ReleaseAndGetAddressOf()),
                           nullptr);
        if (direct_input) {
            enumeration_failure.clear();
            const auto status = direct_input->EnumDevices(DI8DEVCLASS_GAMECTRL, enum_pad, this,
                                                          DIEDFL_ATTACHEDONLY);
            enumeration_failure.invoke([&] {
                if (FAILED(status))
                    throw std::runtime_error("Cannot enumerate DirectInput devices");
            });
            enumeration_failure.rethrow();
        }
    }
    for (const auto& profile : value.input.players) {
        const bool enabled = rxpce_input::device_enabled(profile.device, value.input.xinput_enabled,
                                                         value.input.directinput_enabled);
        if (!enabled && &profile != &value.input.players[player])
            continue;
        const auto found = std::find_if(devices.begin(), devices.end(), [&](const Device& d) {
            return _stricmp(d.id.c_str(), profile.device.c_str()) == 0;
        });
        if (found != devices.end())
            continue;
        const int slot = rxpce_input::xinput_slot(profile.device);
        const auto name = enabled
                              ? rxpce_input::directinput_device_name(to_wide(profile.device), false)
                              : (slot >= 0 ? L"XInput " + std::to_wstring(slot + 1) + L" (Disabled)"
                                           : L"DirectInput: (Disabled) " + to_wide(profile.device));
        devices.push_back({profile.device, name});
    }
    ::SendMessage(GetDlgItem(device), CB_RESETCONTENT, 0, 0);
    for (std::size_t n = 0; n < devices.size(); ++n)
        combo_item(device, devices[n].name, n);
    if (value.input.xinput_enabled)
        refresh_xinput_devices();
}
void SettingsDialog::advance_mapping(int row) {
    if (row + 1 < static_cast<int>(actions.size()))
        mappings[row + 1].SetFocus();
}
void SettingsDialog::close_preview() {
    xinput_preview.reset();
    if (preview) {
        preview->Unacquire();
        preview.Reset();
    }
    std::fill(std::begin(old_buttons), std::end(old_buttons), BYTE{});
}
void SettingsDialog::refresh_xinput_devices() {
    HWND control = GetDlgItem(device);
    if (::SendMessage(control, CB_GETDROPPEDSTATE, 0, 0))
        return;
    const auto selection = ::SendMessage(control, CB_GETCURSEL, 0, 0);
    bool changed = false;
    for (std::size_t n = 0; n < devices.size(); ++n) {
        const int slot = rxpce_input::xinput_slot(devices[n].id);
        if (slot < 0)
            continue;
        rxpce_input::PadSnapshot snapshot;
        const auto name =
            rxpce_input::xinput_device_name(slot, rxpce_input::read_xinput(slot, snapshot, true));
        if (name == devices[n].name)
            continue;
        changed = true;
        devices[n].name = name;
        for (int i = 0; i < ::SendMessage(control, CB_GETCOUNT, 0, 0); ++i)
            if (::SendMessage(control, CB_GETITEMDATA, i, 0) == static_cast<LPARAM>(n)) {
                ::SendMessage(control, CB_DELETESTRING, i, 0);
                const auto inserted = ::SendMessageW(control, CB_INSERTSTRING, i,
                                                     reinterpret_cast<LPARAM>(name.c_str()));
                ::SendMessage(control, CB_SETITEMDATA, inserted, n);
                break;
            }
    }
    if (changed && selection != CB_ERR)
        ::SendMessage(control, CB_SETCURSEL, selection, 0);
    next_xinput_refresh = GetTickCount64() + 2000;
}
void SettingsDialog::update_device() {
    auto& profile = value.input.players[player];
    close_preview();
    profile.device = devices[selected(device)].id;
    keyboard = profile.device == "keyboard";
    xinput_player = rxpce_input::xinput_slot(profile.device);
    actions = rxpce_input::mapping_actions(profile.buttons == 6, keyboard);
    const wchar_t* names[] = {L"I",  L"II",   L"I rapid", L"II rapid", L"Run", L"Select", L"Down",
                              L"Up", L"Left", L"Right",   L"III",      L"IV",  L"V",      L"VI"};
    for (int n = 0; n < static_cast<int>(actions.size()); ++n) {
        ::SetWindowTextW(GetDlgItem(label_base + n), names[actions[n]]);
        mappings[n].ResetContent();
        mappings[n].capture_keyboard = keyboard;
        if (keyboard)
            for (int key = 1; key <= 255; ++key)
                combo_item(map_base + n, key_name(key), key);
        else
            for (int button = 0; button < (xinput_player >= 0 ? 12 : 32); ++button)
                combo_item(map_base + n,
                           xinput_player >= 0 ? rxpce_input::xinput_button_name(button)
                                              : L"Button " + std::to_wstring(button + 1),
                           button);
        const auto& buttons = xinput_player >= 0 ? profile.xinput : profile.pad;
        select_data(map_base + n, keyboard ? profile.keyboard[actions[n]]
                                           : buttons[rxpce_input::pad_binding(actions[n])]);
    }
    const bool supported = rxpce_input::device_enabled(profile.device, value.input.xinput_enabled,
                                                       value.input.directinput_enabled);
    for (int n = 0; n < static_cast<int>(actions.size()); ++n)
        mappings[n].EnableWindow(supported);
    if (supported && !keyboard && xinput_player < 0 && direct_input) {
        GUID id{};
        const auto guid = to_wide(profile.device);
        if (SUCCEEDED(CLSIDFromString(guid.c_str(), &id)) &&
            SUCCEEDED(direct_input->CreateDevice(id, preview.ReleaseAndGetAddressOf(), nullptr))) {
            if (FAILED(preview->SetDataFormat(&c_dfDIJoystick)) ||
                FAILED(preview->SetCooperativeLevel(m_hWnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND)))
                close_preview();
            else
                preview->Acquire();
        }
    }
}
void SettingsDialog::browse(int target, bool folder) {
    if (folder) {
        ATL::CComPtr<IFileOpenDialog> dialog;
        if (FAILED(dialog.CoCreateInstance(CLSID_FileOpenDialog)))
            throw std::runtime_error("Cannot open folder picker.");
        FILEOPENDIALOGOPTIONS options{};
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST |
                           FOS_NOCHANGEDIR);
        if (SUCCEEDED(dialog->Show(m_hWnd))) {
            ATL::CComPtr<IShellItem> item;
            PWSTR path = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) &&
                SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                const auto selected = to_utf8(path);
                CoTaskMemFree(path);
                set_text(target, selected);
            }
        }
    } else {
        std::vector<wchar_t> path(32768);
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = m_hWnd;
        dialog.lpstrFilter = target == bios ? L"System Card (*.pce)\0*.pce\0All files (*.*)\0*.*\0"
                                            : L"Palette (*.pal)\0*.pal\0All files (*.*)\0*.*\0";
        dialog.lpstrFile = path.data();
        dialog.nMaxFile = static_cast<DWORD>(path.size());
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
        if (GetOpenFileNameW(&dialog))
            set_text(target, to_utf8(path.data()));
    }
}
LRESULT SettingsDialog::OnNotify(UINT, WPARAM, LPARAM notification, BOOL& handled) {
    const auto& header = *reinterpret_cast<NMHDR*>(notification);
    if (header.idFrom == 2000 && header.code == TCN_SELCHANGE)
        show_page(static_cast<int>(::SendMessage(GetDlgItem(2000), TCM_GETCURSEL, 0, 0)));
    else
        handled = FALSE;
    return 0;
}
LRESULT SettingsDialog::OnScroll(UINT, WPARAM, LPARAM control, BOOL&) {
    try {

        const int id = ::GetDlgCtrlID(reinterpret_cast<HWND>(control));
        if (id == skip_slider || id == latency_slider)
            set_number(
                id == skip_slider ? skip : latency,
                static_cast<int>(::SendMessage(reinterpret_cast<HWND>(control), TBM_GETPOS, 0, 0)));
        return 0;
    } catch (const std::exception& error) {
        return abort_dialog(error.what());
    } catch (...) {
        return abort_dialog("Unexpected settings failure");
    }
}
LRESULT SettingsDialog::OnTimer(UINT, WPARAM, LPARAM, BOOL&) {
    try {

        if (value.input.xinput_enabled && GetTickCount64() >= next_xinput_refresh)
            refresh_xinput_devices();
        if (!rxpce_input::device_enabled(value.input.players[player].device,
                                         value.input.xinput_enabled,
                                         value.input.directinput_enabled))
            return 0;
        rxpce_input::PadSnapshot snapshot;
        if (xinput_player >= 0) {
            const bool active = ::GetAncestor(::GetForegroundWindow(), GA_ROOTOWNER) ==
                                ::GetAncestor(m_hWnd, GA_ROOTOWNER);
            xinput_preview.read(xinput_player, snapshot, active);
        } else {
            if (!preview)
                return 0;
            DIJOYSTATE native{};
            HRESULT status = preview->Poll();
            if (SUCCEEDED(status))
                status = preview->GetDeviceState(sizeof(native), &native);
            if (FAILED(status)) {
                preview->Acquire();
                std::fill(std::begin(old_buttons), std::end(old_buttons), BYTE{});
                return 0;
            }
            std::copy_n(native.rgbButtons, 32, snapshot.buttons.begin());
        }
        HWND focus = ::GetFocus();
        for (int n = 0; n < static_cast<int>(actions.size()); ++n)
            if (mappings[n].m_hWnd == focus)
                for (int button = 0; button < 32; ++button)
                    if ((snapshot.buttons[button] & 0x80) && !(old_buttons[button] & 0x80)) {
                        mappings[n].SetCurSel(button);
                        advance_mapping(n);
                        break;
                    }
        std::copy_n(snapshot.buttons.begin(), 32, old_buttons);
        return 0;
    } catch (const std::exception& error) {
        return abort_dialog(error.what());
    } catch (...) {
        return abort_dialog("Unexpected settings failure");
    }
}
LRESULT SettingsDialog::OnDestroy(UINT, WPARAM, LPARAM, BOOL&) {
    KillTimer(1);
    close_preview();
    direct_input.Reset();
    return 0;
}
LRESULT SettingsDialog::OnCommand(UINT, WPARAM command, LPARAM, BOOL& handled) {
    const int id = LOWORD(command), code = HIWORD(command);
    if (id == IDCANCEL) {
        EndDialog(IDCANCEL);
        return 0;
    }
    try {
        if (code == MappingCombo::captured && id >= map_base && id < map_base + 12) {
            advance_mapping(id - map_base);
            return 0;
        }
        if (id == IDOK) {
            collect_mappings();
            value.audio.backend = IsDlgButtonChecked(wasapi_backend) ? "wasapi" : "directsound";
            value.video.adapter = selected(adapter);
            value.video.zoom = selected(zoom);
            value.video.auto_frame_skip = IsDlgButtonChecked(auto_skip) != 0;
            value.video.frame_skip = number(skip);
            value.video.palette_file = text(palette);
            value.audio.enabled = IsDlgButtonChecked(sound) != 0;
            value.audio.sample_rate = selected(rate);
            value.audio.channels = selected(channels);
            value.audio.latency_ms = number(latency);
            value.audio.cooperative_level =
                static_cast<int>(::SendMessage(GetDlgItem(coop), TBM_GETPOS, 0, 0));
            value.folders.rom = text(rom);
            value.folders.state = text(state);
            value.advanced.base_clock_hz = number(clock_edit);
            value.advanced.log = IsDlgButtonChecked(log_enabled) != 0;
            value.advanced.show_fps = IsDlgButtonChecked(fps) != 0;
            value.cd.bios_file = text(bios);
            std::string error;
            if (!validate(value, error))
                throw std::runtime_error(error);
            value.advanced.base_clock_hz -= value.advanced.base_clock_hz % 1000;
            EndDialog(IDOK);
        } else if (!initializing && code == CBN_SELCHANGE &&
                   (id == device || id == mode || id == port || id == count)) {
            collect_mappings();
            if (id == count) {
                value.input.count = selected(count);
                player = (std::min)(player, value.input.count - 1);
                list_players();
                load_player();
            } else if (id == port) {
                player = selected(port);
                load_player();
            } else if (id == mode) {
                value.input.players[player].buttons = selected(mode);
                update_device();
            } else {
                value.input.players[player].device = devices[selected(device)].id;
                load_player();
            }
            show_page(2);
        } else if ((id == ds_backend || id == wasapi_backend) && code == BN_CLICKED)
            update_audio_controls();
        else if (!initializing && (id == xinput_support || id == directinput_support) &&
                 code == BN_CLICKED) {
            collect_mappings();
            value.input.xinput_enabled = IsDlgButtonChecked(xinput_support) != 0;
            value.input.directinput_enabled = IsDlgButtonChecked(directinput_support) != 0;
            load_player();
            show_page(4);
        } else if (id == palette_browse)
            browse(palette, false);
        else if (id == bios_browse) {
            browse(bios, false);
            bios_description(m_hWnd, text(bios));
        } else if (id == bios && code == EN_CHANGE && !initializing)
            ::SetWindowTextW(GetDlgItem(bios_info), L"Checked when CD emulation starts");
        else if (id == rom_browse || id == state_browse)
            browse(id == rom_browse ? rom : state, true);
        else if (!initializing && (id == skip || id == latency) && code == EN_CHANGE) {
            try {
                const int n = number(id);
                ::SendMessage(GetDlgItem(id == skip ? skip_slider : latency_slider), TBM_SETPOS,
                              TRUE, n);
            } catch (...) {
            }
        } else
            handled = FALSE;
    } catch (const std::bad_alloc&) {
        return abort_dialog("Not enough memory for settings");
    } catch (const std::exception& e) {
        if (enumeration_failure.failed())
            return abort_dialog(e.what());
        MessageBox(e.what(), "Invalid settings", MB_OK | MB_ICONERROR);
    } catch (...) {
        return abort_dialog("Unexpected settings failure");
    }
    return 0;
}
