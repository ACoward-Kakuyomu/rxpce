#include "wtl_support.h"
#include "MainFrame.h"
#include "file_paths.h"
#include <stdexcept>
WTL::CAppModule _Module;
int WINAPI WinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int show) {
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_WIN95_CLASSES};
    if (!InitCommonControlsEx(&controls) || FAILED(_Module.Init(nullptr, instance))) {
        if (SUCCEEDED(com))
            CoUninitialize();
        return 1;
    }
    WTL::CMessageLoop loop;
    if (!_Module.AddMessageLoop(&loop)) {
        _Module.Term();
        if (SUCCEEDED(com))
            CoUninitialize();
        return 1;
    }
    int result = 1;
    try {
        CMainFrame frame(rxpce_config::config_path(rxpce_paths::executable()));
        auto& info = CMainFrame::GetWndClassInfo();
        info.m_wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        RECT bounds{0, 0, 320, 240};
        HMENU menu = LoadMenu(instance, MAKEINTRESOURCE(IDR_MENU));
        try {
            if (!menu)
                throw std::runtime_error("Cannot create application menu");
            if (frame.Create(nullptr, bounds, "rxpce",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_BORDER | WS_MINIMIZEBOX,
                             0, menu)) {
                menu = nullptr; // Attached menu is now owned by the frame, including fullscreen.
                frame.ShowWindow(show);
                frame.UpdateWindow();
                result = loop.Run();
            } else if (menu && IsMenu(menu)) {
                DestroyMenu(menu);
                menu = nullptr;
            }
        } catch (...) {
            if (frame.IsWindow())
                frame.DestroyWindow();
            if (menu && IsMenu(menu))
                DestroyMenu(menu);
            throw;
        }
    } catch (const std::exception& error) {
        ::MessageBoxA(nullptr, error.what(), "rxpce initialization failed", MB_OK | MB_ICONERROR);
    } catch (...) {
        ::MessageBoxA(nullptr, "Unexpected initialization failure", "rxpce", MB_OK | MB_ICONERROR);
    }
    _Module.RemoveMessageLoop();
    _Module.Term();
    if (SUCCEEDED(com))
        CoUninitialize();
    return result;
}
