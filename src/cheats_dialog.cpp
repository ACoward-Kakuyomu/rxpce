#include "memory_search_dialog.h"
#include "settings.h"
#include <algorithm>
#include <cwchar>

namespace {
std::wstring text(HWND window, int id) {
    wchar_t buffer[300]{};
    ::GetWindowTextW(::GetDlgItem(window, id), buffer, 300);
    return buffer;
}
constexpr int search_controls[] = {IDC_SEARCH_SIZE_LABEL, IDC_SEARCH_VALUE_LABEL, IDC_SEARCH_HELP,
    IDC_SEARCH_SIZE, IDC_SEARCH_CONDITION, IDC_SEARCH_VALUE, IDC_SEARCH_FILTER, IDC_SEARCH_NEW,
    IDC_SEARCH_RESULTS, IDC_SEARCH_STATUS};
constexpr int cheat_controls[] = {IDC_CHEAT_NAME_LABEL, IDC_CHEAT_REGION_LABEL, IDC_CHEAT_OFFSET_LABEL,
    IDC_CHEAT_BYTES_LABEL, IDC_CHEAT_VALUE_LABEL, IDC_CHEAT_NAME, IDC_CHEAT_ENABLED, IDC_CHEAT_REGION,
    IDC_CHEAT_OFFSET, IDC_CHEAT_BYTES, IDC_CHEAT_VALUE, IDC_CHEAT_ALWAYS, IDC_CHEAT_ADD, IDC_CHEAT_UPDATE,
    IDC_CHEAT_DELETE, IDC_CHEAT_APPLY, IDC_CHEAT_LIST, IDC_CHEAT_STATUS};
}
void MemorySearchDialog::SelectTab(int tab) {
    if (pending_) return;
    state_.tab = tab == 1 ? 1 : 0;
    TabCtrl_SetCurSel(GetDlgItem(IDC_TOOLS_TAB), state_.tab);
    for (const auto id : search_controls) ::ShowWindow(GetDlgItem(id), state_.tab == 0 ? SW_SHOW : SW_HIDE);
    for (const auto id : cheat_controls) ::ShowWindow(GetDlgItem(id), state_.tab == 1 ? SW_SHOW : SW_HIDE);
    SendMessage(DM_SETDEFID, state_.tab == 0 ? IDC_SEARCH_FILTER :
                state_.selected ? IDC_CHEAT_UPDATE : IDC_CHEAT_ADD);
    ::SetFocus(GetDlgItem(state_.tab == 0 ? IDC_SEARCH_VALUE : IDC_CHEAT_NAME));
}
void MemorySearchDialog::InitCheats() {
    operation_status_ = state_.warning;
    for (const auto label : {L"Search", L"Cheats"}) {
        TCITEMW item{};
        item.mask = TCIF_TEXT;
        item.pszText = const_cast<wchar_t*>(label);
        ::SendDlgItemMessageW(m_hWnd, IDC_TOOLS_TAB, TCM_INSERTITEMW, TabCtrl_GetItemCount(GetDlgItem(IDC_TOOLS_TAB)),
                              reinterpret_cast<LPARAM>(&item));
    }
    for (const auto& region : search_.current().regions) {
        if (!rxpce_search::writable_bytes(region.id)) continue;
        regions_.push_back(region.id);
        const auto label = rxpce_config::to_wide(region.name);
        ::SendDlgItemMessageW(m_hWnd, IDC_CHEAT_REGION, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    }
    for (const auto label : {L"1", L"2", L"4"})
        ::SendDlgItemMessageW(m_hWnd, IDC_CHEAT_BYTES, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
    ::SendDlgItemMessage(m_hWnd, IDC_CHEAT_NAME, EM_SETLIMITTEXT, 128, 0);
    ::SendDlgItemMessage(m_hWnd, IDC_CHEAT_OFFSET, EM_SETLIMITTEXT, 32, 0);
    ::SendDlgItemMessage(m_hWnd, IDC_CHEAT_VALUE, EM_SETLIMITTEXT, 32, 0);
    const auto list = GetDlgItem(IDC_CHEAT_LIST);
    ListView_SetUnicodeFormat(list, TRUE);
    ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    const wchar_t* columns[] = {L"Enabled", L"Name", L"Region", L"Offset", L"Bytes", L"Value", L"Always Apply"};
    const int widths[] = {58, 140, 105, 82, 42, 170, 90};
    for (int i = 0; i < 7; ++i) {
        LVCOLUMNW column{};
        column.mask = LVCF_TEXT | LVCF_WIDTH;
        column.pszText = const_cast<wchar_t*>(columns[i]);
        column.cx = MulDiv(widths[i], GetDpiForWindow(m_hWnd), 96);
        ::SendMessageW(list, LVM_INSERTCOLUMNW, i, reinterpret_cast<LPARAM>(&column));
    }
    RefreshCheats();
    FillForm();
}
void MemorySearchDialog::FillForm() {
    filling_ = true;
    rxpce_cheats::Entry entry;
    const auto& list = state_.editor.document().cheats;
    if (state_.selected && *state_.selected < list.size()) entry = list[*state_.selected];
    else state_.selected.reset();
    ::SetDlgItemTextW(m_hWnd, IDC_CHEAT_NAME, rxpce_config::to_wide(entry.name).c_str());
    ::SetDlgItemTextW(m_hWnd, IDC_CHEAT_OFFSET, std::to_wstring(entry.offset).c_str());
    ::SetDlgItemTextW(m_hWnd, IDC_CHEAT_VALUE, std::to_wstring(entry.value).c_str());
    CheckDlgButton(IDC_CHEAT_ENABLED, entry.enabled ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_CHEAT_ALWAYS, entry.always_apply ? BST_CHECKED : BST_UNCHECKED);
    const auto found = std::find(regions_.begin(), regions_.end(), entry.region);
    ::SendDlgItemMessage(m_hWnd, IDC_CHEAT_REGION, CB_SETCURSEL, found - regions_.begin(), 0);
    ::SendDlgItemMessage(m_hWnd, IDC_CHEAT_BYTES, CB_SETCURSEL, entry.bytes == 4 ? 2 : entry.bytes == 2 ? 1 : 0, 0);
    form_dirty_ = false;
    filling_ = false;
}
rxpce_cheats::Entry MemorySearchDialog::ReadEntry() {
    rxpce_cheats::Entry entry;
    entry.name = rxpce_config::to_utf8(text(m_hWnd, IDC_CHEAT_NAME));
    const auto region = ::SendDlgItemMessage(m_hWnd, IDC_CHEAT_REGION, CB_GETCURSEL, 0, 0);
    const auto size = ::SendDlgItemMessage(m_hWnd, IDC_CHEAT_BYTES, CB_GETCURSEL, 0, 0);
    if (region < 0 || static_cast<std::size_t>(region) >= regions_.size() || size < 0 || size > 2)
        throw std::invalid_argument("Select a RAM region and byte size");
    entry.region = regions_[region];
    entry.bytes = 1u << static_cast<unsigned>(size);
    if (!rxpce_search::parse_value(text(m_hWnd, IDC_CHEAT_OFFSET), 4, entry.offset) ||
        !rxpce_search::parse_value(text(m_hWnd, IDC_CHEAT_VALUE), entry.bytes, entry.value))
        throw std::invalid_argument("Enter a valid unsigned offset and value in decimal or 0x hexadecimal");
    entry.enabled = IsDlgButtonChecked(IDC_CHEAT_ENABLED) == BST_CHECKED;
    entry.always_apply = IsDlgButtonChecked(IDC_CHEAT_ALWAYS) == BST_CHECKED;
    return entry;
}
bool MemorySearchDialog::StoreForm(bool add) {
    if (!add && !state_.selected) throw std::invalid_argument("Select a cheat to update");
    std::string error;
    const auto entry = ReadEntry();
    if (!state_.editor.store(entry, add ? std::nullopt : state_.selected, search_.current(), error))
        throw std::invalid_argument(error);
    if (add) state_.selected = state_.editor.document().cheats.size() - 1;
    form_dirty_ = false;
    return true;
}
bool MemorySearchDialog::ResolveForm() {
    if (!form_dirty_) return true;
    const auto choice = MessageBox("Keep the field edits in the cheat list?", "Uncommitted cheat fields",
                                   MB_YESNOCANCEL | MB_ICONQUESTION);
    if (choice == IDCANCEL) return false;
    if (choice == IDYES) StoreForm(!state_.selected);
    else FillForm();
    RefreshCheats();
    return true;
}
void MemorySearchDialog::RefreshCheats() {
    const bool was_filling = filling_;
    filling_ = true;
    const auto list = GetDlgItem(IDC_CHEAT_LIST);
    ListView_DeleteAllItems(list);
    const auto& entries = state_.editor.document().cheats;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto& entry = entries[i];
        const auto found = std::find_if(search_.current().regions.begin(), search_.current().regions.end(),
            [&](const auto& region) { return region.id == entry.region; });
        const auto region = found == search_.current().regions.end() ? std::string{} : found->name;
        wchar_t offset[24]{};
        std::swprintf(offset, 24, L"0x%05X", static_cast<unsigned>(entry.offset));
        const std::wstring columns[] = {entry.enabled ? L"Yes" : L"No", rxpce_config::to_wide(entry.name),
            rxpce_config::to_wide(region), offset, std::to_wstring(entry.bytes),
            rxpce_search::format_value(entry.value, entry.bytes), entry.always_apply ? L"Yes" : L"No"};
        for (int subitem = 0; subitem < 7; ++subitem) {
            LVITEMW item{};
            item.mask = LVIF_TEXT;
            item.iItem = static_cast<int>(i);
            item.iSubItem = subitem;
            item.pszText = const_cast<wchar_t*>(columns[subitem].c_str());
            ::SendMessageW(list, subitem == 0 ? LVM_INSERTITEMW : LVM_SETITEMTEXTW, i, reinterpret_cast<LPARAM>(&item));
        }
    }
    if (state_.selected && *state_.selected < entries.size())
        ListView_SetItemState(list, static_cast<int>(*state_.selected), LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    for (const auto id : cheat_controls) ::EnableWindow(GetDlgItem(id), !pending_);
    for (const auto id : search_controls) ::EnableWindow(GetDlgItem(id), !pending_);
    ::EnableWindow(GetDlgItem(IDC_TOOLS_TAB), !pending_);
    ::EnableWindow(GetDlgItem(IDOK), !pending_);
    ::EnableWindow(GetDlgItem(IDC_SEARCH_VALUE), !pending_ && state_.condition == 0);
    ::EnableWindow(GetDlgItem(IDC_CHEAT_SAVE), !pending_ && bool(actions_.save) &&
                   (state_.editor.dirty() || form_dirty_ || state_.overwrite_confirmation));
    ::EnableWindow(GetDlgItem(IDC_CHEAT_UPDATE), !pending_ && state_.selected.has_value());
    ::EnableWindow(GetDlgItem(IDC_CHEAT_DELETE), !pending_ && state_.selected.has_value());
    if (state_.tab == 1)
        SendMessage(DM_SETDEFID, state_.selected ? IDC_CHEAT_UPDATE : IDC_CHEAT_ADD);
    const bool manual = state_.selected && *state_.selected < entries.size() &&
                        entries[*state_.selected].enabled && !entries[*state_.selected].always_apply;
    ::EnableWindow(GetDlgItem(IDC_CHEAT_APPLY), !pending_ && bool(actions_.apply) && manual && !form_dirty_);
    const auto status = state_.editor.dirty() ? L"Unsaved definitions" : L"Definitions unchanged";
    ::SetDlgItemTextW(m_hWnd, IDC_CHEAT_STATUS, operation_status_.empty() ? status : rxpce_config::to_wide(operation_status_).c_str());
    filling_ = was_filling;
}
void MemorySearchDialog::BeginSave() {
    if (!ResolveForm()) { CancelClose(); return; }
    if (state_.overwrite_confirmation && MessageBox(
            "The existing cheat file could not be loaded. Replace it with these definitions?",
            "Replace invalid cheat file", MB_YESNO | MB_ICONWARNING) != IDYES) {
        CancelClose();
        return;
    }
    if (!actions_.save) throw std::runtime_error("Saving is not available for this session");
    std::string error;
    if (!rxpce_cheats::validate(state_.editor.document(), search_.current(), error)) throw std::invalid_argument(error);
    pending_ = true;
    operation_status_ = "Saving cheat definitions...";
    RefreshCheats();
    actions_.save(state_.editor.document());
}
void MemorySearchDialog::CancelClose() {
    close_after_save_ = false;
    if (actions_.cancel_close) actions_.cancel_close();
}
void MemorySearchDialog::CompleteOperation(std::optional<rxpce_search::Snapshot> snapshot,
                                          const std::optional<rxpce_cheats::Document>& saved,
                                          const std::string& error) {
    pending_ = false;
    try {
        if (!error.empty()) throw std::runtime_error(error);
        if (snapshot) search_.refresh_current(std::move(*snapshot));
        if (saved) {
            state_.editor.reset(*saved);
            state_.overwrite_confirmation = false;
            state_.warning.clear();
        }
        operation_status_ = saved ? "Cheat definitions saved" : "Applied to RAM; emulation remains paused";
        RefreshResults();
        RefreshCheats();
        if (close_after_save_) {
            close_after_save_ = false;
            PostMessage(WM_CLOSE);
        }
    } catch (const std::exception& failure) {
        CancelClose();
        operation_status_ = failure.what();
        RefreshCheats();
        MessageBox(failure.what(), "Memory Tools", MB_OK | MB_ICONERROR);
    }
}
void MemorySearchDialog::CheatCommand(WORD id, WORD notification) {
    if (filling_) return;
    if ((id <= IDC_CHEAT_ALWAYS) &&
        (notification == EN_CHANGE || notification == CBN_SELCHANGE || notification == BN_CLICKED)) {
        form_dirty_ = true;
        ::EnableWindow(GetDlgItem(IDC_CHEAT_SAVE), !pending_ && bool(actions_.save));
        ::EnableWindow(GetDlgItem(IDC_CHEAT_APPLY), FALSE);
        ::SetDlgItemTextW(m_hWnd, IDC_CHEAT_STATUS, L"Uncommitted field edits");
        return;
    }
    if (id == IDC_CHEAT_ADD || id == IDC_CHEAT_UPDATE) {
        StoreForm(id == IDC_CHEAT_ADD);
        operation_status_.clear();
        RefreshCheats();
        FillForm();
    } else if (id == IDC_CHEAT_DELETE && state_.selected) {
        state_.editor.erase(*state_.selected);
        state_.selected.reset();
        operation_status_.clear();
        form_dirty_ = false;
        RefreshCheats();
        FillForm();
    } else if (id == IDC_CHEAT_SAVE) {
        BeginSave();
    } else if (id == IDC_CHEAT_APPLY && actions_.apply && state_.selected && !form_dirty_) {
        const auto& entry = state_.editor.document().cheats.at(*state_.selected);
        if (!entry.enabled || entry.always_apply) throw std::invalid_argument("Select an enabled single-use cheat");
        pending_ = true;
        operation_status_ = "Applying to RAM...";
        RefreshCheats();
        actions_.apply(entry);
    }
}
LRESULT MemorySearchDialog::CheatNotify(NMHDR* header) {
    if (filling_ || pending_) return 0;
    if (header->idFrom == IDC_TOOLS_TAB) {
        if (header->code == TCN_SELCHANGING) return ResolveForm() ? FALSE : TRUE;
        if (header->code == TCN_SELCHANGE) SelectTab(TabCtrl_GetCurSel(header->hwndFrom));
    } else if (header->idFrom == IDC_SEARCH_RESULTS && header->code == NM_DBLCLK) {
        const auto row = reinterpret_cast<NMITEMACTIVATE*>(header)->iItem;
        if (row >= 0 && static_cast<std::size_t>(row) < search_.candidates().size() && ResolveForm()) {
            state_.selected = state_.editor.from_search(search_, search_.candidates()[row]);
            RefreshCheats();
            FillForm();
            SelectTab(1);
        }
    } else if (header->idFrom == IDC_CHEAT_LIST && header->code == LVN_ITEMCHANGED) {
        const auto& change = *reinterpret_cast<NMLISTVIEW*>(header);
        if ((change.uChanged & LVIF_STATE) && (change.uNewState & LVIS_SELECTED))
            PostMessage(WM_APP + 1);
    }
    return 0;
}
LRESULT MemorySearchDialog::OnCheatSelection(UINT, WPARAM, LPARAM, BOOL&) {
    if (pending_ || filling_) return 0;
    const auto row = ListView_GetNextItem(GetDlgItem(IDC_CHEAT_LIST), -1, LVNI_SELECTED);
    if (row < 0 || state_.selected == static_cast<std::size_t>(row)) return 0;
    try {
        if (ResolveForm()) {
            state_.selected = static_cast<std::size_t>(row);
            FillForm();
        }
        RefreshCheats();
    } catch (const std::exception& error) {
        MessageBox(error.what(), "Memory Tools", MB_OK | MB_ICONERROR);
        filling_ = true;
        ListView_SetItemState(GetDlgItem(IDC_CHEAT_LIST), -1, 0, LVIS_SELECTED);
        if (state_.selected)
            ListView_SetItemState(GetDlgItem(IDC_CHEAT_LIST), static_cast<int>(*state_.selected), LVIS_SELECTED, LVIS_SELECTED);
        filling_ = false;
    }
    return 0;
}
