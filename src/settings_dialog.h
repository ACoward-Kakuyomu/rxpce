#pragma once
#include "wtl_support.h"
#include "settings.h"
#include "callback_boundary.h"
#include "xinput_state.h"
#include <dinput.h>
#include <vector>
#include <wrl/client.h>

class MappingCombo : public ATL::CWindowImpl<MappingCombo, WTL::CComboBox> {
  public:
    bool capture_keyboard = true;
    enum { captured = 0x7001 };
    BEGIN_MSG_MAP(MappingCombo)
    MESSAGE_HANDLER(WM_GETDLGCODE, OnCode)
    MESSAGE_HANDLER(WM_KEYDOWN, OnKey)
    MESSAGE_HANDLER(WM_SYSKEYDOWN, OnKey)
    END_MSG_MAP()
    LRESULT OnCode(UINT, WPARAM, LPARAM, BOOL& handled) {
        if (!capture_keyboard) {
            handled = FALSE;
            return 0;
        }
        return DLGC_WANTALLKEYS;
    }
    LRESULT OnKey(UINT message, WPARAM key, LPARAM flags, BOOL& handled) {
        if (!capture_keyboard || (message == WM_SYSKEYDOWN && key == VK_F4)) {
            handled = FALSE;
            return 0;
        }
        if (!(flags & (LPARAM(1) << 30))) {
            for (int i = 0; i < GetCount(); ++i)
                if (GetItemData(i) == key) {
                    SetCurSel(i);
                    ::SendMessage(GetParent(), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(), captured),
                                  reinterpret_cast<LPARAM>(m_hWnd));
                    break;
                }
        }
        return 0;
    }
};
class SettingsDialog : public ATL::CDialogImpl<SettingsDialog> {
  public:
    enum { IDD = IDD_SETTINGS };
    rxpce_config::Settings value;
    explicit SettingsDialog(const rxpce_config::Settings& settings) : value(settings) {}
    BEGIN_MSG_MAP(SettingsDialog)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInit)
    MESSAGE_HANDLER(WM_NOTIFY, OnNotify)
    MESSAGE_HANDLER(WM_HSCROLL, OnScroll)
    MESSAGE_HANDLER(WM_TIMER, OnTimer)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    MESSAGE_HANDLER(WM_COMMAND, OnCommand)
    END_MSG_MAP()
  private:
    struct Device {
        std::string id;
        std::wstring name;
    };
    std::vector<Device> devices;
    rxpce_host::CallbackFailure enumeration_failure;
    LRESULT abort_dialog(const char* error) noexcept;
    std::vector<HWND> pages[5];
    MappingCombo mappings[12];
    std::vector<int> actions;
    int player = 0;
    int xinput_player = -1;
    rxpce_input::XInputPoller xinput_preview;
    ULONGLONG next_xinput_refresh = 0;
    Microsoft::WRL::ComPtr<IDirectInput8A> direct_input;
    Microsoft::WRL::ComPtr<IDirectInputDevice8A> preview;
    BYTE old_buttons[32]{};
    bool keyboard = true, initializing = true;
    HWND add(int page, const wchar_t* cls, const wchar_t* title, int id, int x, int y, int width,
             int height, DWORD style = 0, DWORD ex = 0);
    void label(int page, const wchar_t* title, int x, int y, int width = 100);
    void combo_item(int id, const std::wstring& text, LPARAM data);
    void select_data(int id, LPARAM data);
    int selected(int id);
    int number(int id);
    void set_number(int id, int value);
    void set_text(int id, const std::string& utf8);
    std::string text(int id);
    void slider(int page, int id, int x, int y, int width, int low, int high, int current);
    void show_page(int page);
    void collect_mappings();
    void update_device();
    void rebuild_devices();
    void advance_mapping(int row);
    void update_audio_controls();
    void refresh_xinput_devices();
    void load_player();
    void list_players();
    void close_preview();
    void browse(int target, bool folder);
    static BOOL CALLBACK enum_pad(const DIDEVICEINSTANCEA*, void*);
    LRESULT OnInit(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnNotify(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnScroll(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnTimer(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnCommand(UINT, WPARAM, LPARAM, BOOL&);
};
