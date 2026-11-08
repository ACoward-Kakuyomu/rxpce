#include "memory_search_dialog.h"
#include <cstdio>

bool MemorySearchDialog::ProcessMessage(MSG* message) {
    if (message->message == WM_SYSKEYUP && message->wParam == VK_MENU)
        return true;
    if ((message->message == WM_SYSKEYDOWN || message->message == WM_SYSKEYUP) && message->wParam == VK_F9) {
        if (message->message == WM_SYSKEYDOWN && !(message->lParam & (LPARAM(1) << 30)))
            SelectTab(1);
        return true;
    }
    if ((message->message == WM_KEYDOWN || message->message == WM_KEYUP) &&
        message->wParam == VK_F9) {
        if (message->message == WM_KEYDOWN && !(message->lParam & (LPARAM(1) << 30)))
            if (!pending_) PostMessage(WM_CLOSE);
        return true;
    }
    return IsDialogMessage(message) != FALSE;
}
void MemorySearchDialog::RememberQuery() {
    wchar_t text[64]{};
    ::GetWindowTextW(GetDlgItem(IDC_SEARCH_VALUE), text, 64);
    state_.value = text;
    state_.condition = static_cast<int>(::SendDlgItemMessage(m_hWnd, IDC_SEARCH_CONDITION, CB_GETCURSEL, 0, 0));
}
LRESULT MemorySearchDialog::OnInit(UINT, WPARAM, LPARAM, BOOL&) {
    try {
        for (const auto text : {L"1 byte", L"2 bytes", L"4 bytes"})
            ::SendDlgItemMessageW(m_hWnd, IDC_SEARCH_SIZE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
        const int size = search_.width() == 4 ? 2 : search_.width() == 2 ? 1 : 0;
        ::SendDlgItemMessage(m_hWnd, IDC_SEARCH_SIZE, CB_SETCURSEL, size, 0);
        const wchar_t* conditions[] = {L"Value equals", L"Unchanged", L"Changed", L"Increased", L"Decreased"};
        const int count = search_.has_previous() ? 5 : 1;
        for (int i = 0; i < count; ++i)
            ::SendDlgItemMessageW(m_hWnd, IDC_SEARCH_CONDITION, CB_ADDSTRING, 0,
                                  reinterpret_cast<LPARAM>(conditions[i]));
        if (state_.condition < 0 || state_.condition >= count)
            state_.condition = 0;
        ::SendDlgItemMessage(m_hWnd, IDC_SEARCH_CONDITION, CB_SETCURSEL, state_.condition, 0);
        ::SendDlgItemMessage(m_hWnd, IDC_SEARCH_VALUE, EM_SETLIMITTEXT, 32, 0);
        ::SetDlgItemTextW(m_hWnd, IDC_SEARCH_VALUE, state_.value.c_str());
        ::EnableWindow(GetDlgItem(IDC_SEARCH_VALUE), state_.condition == 0);
        const auto list = GetDlgItem(IDC_SEARCH_RESULTS);
        ListView_SetUnicodeFormat(list, TRUE);
        ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
        const wchar_t* columns[] = {L"Region", L"Offset", L"Current (dec / hex)", L"Previous (dec / hex)"};
        const int widths[] = {135, 90, 170, 170};
        for (int i = 0; i < 4; ++i) {
            LVCOLUMNW column{};
            column.mask = LVCF_TEXT | LVCF_WIDTH;
            column.pszText = const_cast<wchar_t*>(columns[i]);
            column.cx = MulDiv(widths[i], GetDpiForWindow(m_hWnd), 96);
            ::SendMessageW(list, LVM_INSERTCOLUMNW, i, reinterpret_cast<LPARAM>(&column));
        }
        RefreshResults();
        InitCheats();
        SelectTab(state_.tab);
        CenterWindow(GetParent());
    } catch (const std::exception& error) {
        pending_ = false;
        close_after_save_ = false;
        MessageBox(error.what(), "Memory Tools", MB_OK | MB_ICONERROR);
        DestroyWindow();
    }
    return TRUE;
}
void MemorySearchDialog::RefreshResults() {
    const auto count = search_.candidates().size();
    ListView_SetItemCountEx(GetDlgItem(IDC_SEARCH_RESULTS), static_cast<int>(count), LVSICF_NOSCROLL);
    if (count)
        ListView_EnsureVisible(GetDlgItem(IDC_SEARCH_RESULTS), 0, FALSE);
    ::InvalidateRect(GetDlgItem(IDC_SEARCH_RESULTS), nullptr, TRUE);
    const auto status = L"Candidates: " + std::to_wstring(count) +
        (search_.has_previous() ? L". Comparing with the previous F9 capture."
                               : L". Resume and press F9 again to compare.");
    ::SetDlgItemTextW(m_hWnd, IDC_SEARCH_STATUS, status.c_str());
}
LRESULT MemorySearchDialog::OnCommand(UINT, WPARAM wparam, LPARAM, BOOL&) {
    try {
        const auto id = LOWORD(wparam);
        if (pending_) return 0;
        if (id >= IDC_CHEAT_NAME && id <= IDC_CHEAT_SAVE) {
            CheatCommand(id, HIWORD(wparam));
            return 0;
        }
        if (id == IDOK || id == IDCANCEL) {
            SendMessage(WM_CLOSE);
        } else if (id == IDC_SEARCH_CONDITION && HIWORD(wparam) == CBN_SELCHANGE) {
            RememberQuery();
            ::EnableWindow(GetDlgItem(IDC_SEARCH_VALUE), state_.condition == 0);
        } else if (id == IDC_SEARCH_NEW ||
                   (id == IDC_SEARCH_SIZE && HIWORD(wparam) == CBN_SELCHANGE)) {
            const auto size = ::SendDlgItemMessage(m_hWnd, IDC_SEARCH_SIZE, CB_GETCURSEL, 0, 0);
            search_.reset(1u << static_cast<unsigned>(size));
            RefreshResults();
        } else if (id == IDC_SEARCH_FILTER) {
            RememberQuery();
            std::uint32_t value = 0;
            if (state_.condition == 0 && !rxpce_search::parse_value(state_.value, search_.width(), value)) {
                MessageBox("Enter an unsigned value in decimal or hexadecimal (0x prefix) that fits the selected size.",
                           "Invalid search value", MB_OK | MB_ICONWARNING);
                return 0;
            }
            search_.filter(static_cast<rxpce_search::Condition>(state_.condition), value);
            RefreshResults();
        }
    } catch (const std::exception& error) {
        pending_ = false;
        close_after_save_ = false;
        RefreshCheats();
        MessageBox(error.what(), "Memory Tools", MB_OK | MB_ICONERROR);
    }
    return 0;
}
LRESULT MemorySearchDialog::OnNotify(UINT, WPARAM, LPARAM parameter, BOOL&) {
    auto* header = reinterpret_cast<NMHDR*>(parameter);
    if (header->idFrom == IDC_TOOLS_TAB || header->idFrom == IDC_CHEAT_LIST || header->code == NM_DBLCLK) {
        try { return CheatNotify(header); }
        catch (const std::exception& error) {
            MessageBox(error.what(), "Memory Tools", MB_OK | MB_ICONERROR);
            if (header->idFrom == IDC_TOOLS_TAB && header->code == TCN_SELCHANGING)
                return TRUE;
            if (header->idFrom == IDC_CHEAT_LIST) {
                filling_ = true;
                ListView_SetItemState(header->hwndFrom, -1, 0, LVIS_SELECTED);
                if (state_.selected)
                    ListView_SetItemState(header->hwndFrom, static_cast<int>(*state_.selected), LVIS_SELECTED, LVIS_SELECTED);
                filling_ = false;
            }
        }
        return 0;
    }
    if (header->idFrom != IDC_SEARCH_RESULTS || header->code != LVN_GETDISPINFOW)
        return 0;
    auto& item = reinterpret_cast<NMLVDISPINFOW*>(parameter)->item;
    if (!(item.mask & LVIF_TEXT) || item.iItem < 0 ||
        static_cast<std::size_t>(item.iItem) >= search_.candidates().size())
        return 0;
    try {
        const auto candidate = search_.candidates()[item.iItem];
        std::wstring text;
        if (item.iSubItem == 0) {
            const auto& name = search_.current().regions[candidate.region].name;
            text.assign(name.begin(), name.end());
        } else if (item.iSubItem == 1) {
            wchar_t offset[24]{};
            std::swprintf(offset, 24, L"0x%05X", static_cast<unsigned>(candidate.offset));
            text = offset;
        } else if (item.iSubItem == 2) {
            text = rxpce_search::format_value(search_.value(candidate), search_.width());
        } else {
            text = search_.has_previous()
                       ? rxpce_search::format_value(search_.previous_value(candidate), search_.width())
                       : L"-";
        }
        ::lstrcpynW(item.pszText, text.c_str(), item.cchTextMax);
    } catch (const std::exception&) {
        if (item.cchTextMax)
            item.pszText[0] = L'\0';
    }
    return 0;
}
LRESULT MemorySearchDialog::OnClose(UINT, WPARAM, LPARAM, BOOL&) {
    try {
        if (pending_ || !ResolveForm()) { CancelClose(); return 0; }
        RememberQuery();
        if (state_.editor.dirty()) {
            const int choice = MessageBox("Save the cheat definitions before closing this window?\nApplied RAM changes are retained.",
                                          "Unsaved cheats", MB_YESNOCANCEL | MB_ICONQUESTION);
            if (choice == IDCANCEL) { CancelClose(); return 0; }
            if (choice == IDYES) { close_after_save_ = true; BeginSave(); return 0; }
            state_.editor.discard();
            state_.selected.reset();
        }
        DestroyWindow();
    } catch (const std::exception& error) {
        pending_ = false;
        CancelClose();
        RefreshCheats();
        MessageBox(error.what(), "Memory Tools", MB_OK | MB_ICONERROR);
    }
    return 0;
}
LRESULT MemorySearchDialog::OnDestroy(UINT, WPARAM, LPARAM, BOOL&) {
    ::PostMessage(GetParent(), WM_APP + 11, 0, 0);
    return 0;
}
