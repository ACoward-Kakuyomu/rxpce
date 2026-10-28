#define NOMINMAX
#include "cd_prepare_dialog.h"
#include <shlobj.h>
#include <wrl/client.h>
#include <thread>
#include <exception>

void StopCdProgressDialog(HWND owner, IProgressDialog* dialog, bool owner_was_enabled) {
    HWND popup = nullptr;
    Microsoft::WRL::ComPtr<IOleWindow> window;
    if (SUCCEEDED(dialog->QueryInterface(IID_PPV_ARGS(&window))))
        window->GetWindow(&popup);
    const auto foreground = GetForegroundWindow();
    const bool valid = owner && IsWindow(owner);
    const bool active =
        foreground && valid &&
        (GetAncestor(foreground, GA_ROOTOWNER) == GetAncestor(owner, GA_ROOTOWNER) ||
         foreground == popup || (popup && IsChild(popup, foreground)));
    const bool restore = active && owner_was_enabled && IsWindowVisible(owner) && !IsIconic(owner);
    // Re-enable/activate the owner before destroying its foreground popup.
    // Otherwise Windows may choose a different application's window instead.
    if (valid && owner_was_enabled)
        EnableWindow(owner, TRUE);
    if (restore) {
        const auto current = GetForegroundWindow();
        if (current == foreground || current == owner) {
            SetForegroundWindow(owner);
            if (GetForegroundWindow() == owner)
                SetFocus(owner);
        }
    }
    dialog->StopProgressDialog();
}

bool PrepareCdWithProgress(HWND owner, const std::wstring& image, const std::wstring& bios,
                           std::atomic_bool& cancel, std::unique_ptr<rxpce_cd::Session>& result,
                           std::string& error) {
    Microsoft::WRL::ComPtr<IProgressDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_ProgressDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dialog)))) {
        error = "Cannot create CD loading progress dialog";
        return false;
    }
    dialog->SetTitle(L"Opening CD image");
    dialog->SetLine(1, L"Checking BIOS and validating the complete disc...", FALSE, nullptr);
    dialog->SetCancelMsg(L"Cancelling CD preparation...", nullptr);
    const bool owner_was_enabled = IsWindowEnabled(owner) != FALSE;
    dialog->StartProgressDialog(owner, nullptr, PROGDLG_MODAL | PROGDLG_NOTIME | PROGDLG_NOMINIMIZE,
                                nullptr);
    std::atomic_bool finished{false};
    std::atomic<std::uint64_t> done{0}, total{0};
    bool success = false;
    std::thread worker;
    try {
        worker = std::thread([&] {
            try {
                success = rxpce_cd::prepare(image, bios, result, error,
                                            [&](std::uint64_t d, std::uint64_t t) {
                                                total = t;
                                                done = d;
                                                return !cancel.load();
                                            });
            } catch (const std::exception& ex) {
                error = ex.what();
            }
            finished = true;
        });
    } catch (const std::exception& ex) {
        error = ex.what();
        StopCdProgressDialog(owner, dialog.Get(), owner_was_enabled);
        return false;
    }
    bool quit = false;
    WPARAM exit_code = 0;
    while (!finished.load()) {
        if (dialog->HasUserCancelled())
            cancel = true;
        dialog->SetProgress64(done.load(), total.load());
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 30, QS_ALLINPUT);
        MSG message;
        while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                quit = true;
                exit_code = message.wParam;
                cancel = true;
            } else {
                TranslateMessage(&message);
                DispatchMessage(&message);
            }
        }
    }
    worker.join();
    if (dialog->HasUserCancelled())
        cancel = true;
    StopCdProgressDialog(owner, dialog.Get(), owner_was_enabled);
    if (quit)
        PostQuitMessage(static_cast<int>(exit_code));
    if (cancel.load() || quit) {
        result.reset();
        error = "CD preparation cancelled";
        return false;
    }
    return success;
}
