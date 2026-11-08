#pragma once
#include "wtl_support.h"
#include "memory_search.h"
#include "cheats.h"
#include <functional>
#include <utility>

struct MemorySearchDialogState {
    std::wstring value = L"0";
    int condition = 0;
    int tab = 0;
    rxpce_cheats::Editor editor;
    std::optional<std::size_t> selected;
    bool overwrite_confirmation = false;
    std::string warning;
};
struct CheatActions {
    std::function<void(const rxpce_cheats::Document&)> save;
    std::function<void(const rxpce_cheats::Entry&)> apply;
    std::function<void()> cancel_close;
};
class MemorySearchDialog : public ATL::CDialogImpl<MemorySearchDialog> {
  public:
    enum { IDD = IDD_MEMORY_SEARCH };
    MemorySearchDialog(rxpce_search::Search& search, MemorySearchDialogState& state,
                       CheatActions actions = {})
        : search_(search), state_(state), actions_(std::move(actions)) {}
    bool ProcessMessage(MSG* message);
    void CompleteOperation(std::optional<rxpce_search::Snapshot> snapshot,
                           const std::optional<rxpce_cheats::Document>& saved, const std::string& error);
    BEGIN_MSG_MAP(MemorySearchDialog)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInit)
    MESSAGE_HANDLER(WM_COMMAND, OnCommand)
    MESSAGE_HANDLER(WM_NOTIFY, OnNotify)
    MESSAGE_HANDLER(WM_CLOSE, OnClose)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    MESSAGE_HANDLER(WM_APP + 1, OnCheatSelection)
    END_MSG_MAP()
  private:
    rxpce_search::Search& search_;
    MemorySearchDialogState& state_;
    CheatActions actions_;
    bool filling_ = false, form_dirty_ = false, pending_ = false, close_after_save_ = false;
    std::string operation_status_;
    std::vector<rxpce_search::RegionId> regions_;
    void InitCheats();
    void SelectTab(int tab);
    void FillForm();
    rxpce_cheats::Entry ReadEntry();
    bool ResolveForm();
    bool StoreForm(bool add);
    void RefreshCheats();
    void BeginSave();
    void CancelClose();
    void CheatCommand(WORD id, WORD notification);
    LRESULT CheatNotify(NMHDR* header);
    void RefreshResults();
    void RememberQuery();
    LRESULT OnInit(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnCommand(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnNotify(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnCheatSelection(UINT, WPARAM, LPARAM, BOOL&);
};
