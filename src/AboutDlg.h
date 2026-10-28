#pragma once
#include "wtl_support.h"
class CAboutDlg : public ATL::CDialogImpl<CAboutDlg> {
  public:
    enum { IDD = IDD_ABOUT };
    BEGIN_MSG_MAP(CAboutDlg)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    COMMAND_ID_HANDLER(IDOK, OnClose)
    COMMAND_ID_HANDLER(IDCANCEL, OnClose)
    END_MSG_MAP()
    LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
        CenterWindow(GetParent());
        return TRUE;
    }
    LRESULT OnClose(WORD, WORD id, HWND, BOOL&) {
        EndDialog(id);
        return 0;
    }
};
