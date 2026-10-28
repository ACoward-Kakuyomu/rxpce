#include "win32_support.h"
#include "video.h"
#include "d3d11_renderer.h"
#include "Info.h"
#include "log.h"
#include <dxgi.h>
#include <fstream>

HWND hwnd = nullptr;
std::atomic_bool bActiveApp{false}, bFullScreen{false};
VideoSettings video_settings;
namespace {
rxpce_video::Renderer renderer;
rxpce_video::Palette palette;
std::vector<std::uint32_t> frame;
std::atomic_int zoom{1};
int source_width = 320, source_height = 240; // UI thread only.
RECT saved_window = {};
LONG_PTR saved_style = 0;
LONG_PTR saved_exstyle = 0;
HMENU saved_menu = nullptr;
int last_width = 0, last_height = 0; // Emulation thread only.
bool device_lost = false;
HRESULT last_error = S_OK;
} // namespace

BOOL InitializeVideo() {
    last_width = last_height = 0;
    device_lost = false;
    last_error = S_OK;
    palette = rxpce_video::linear_palette();
    if (!video_settings.palette_file.empty()) {
        std::ifstream input(video_settings.palette_file, std::ios::binary);
        if (!rxpce_video::load_palette(input, palette))
            LogDumpDetail("Invalid/unreadable palette, using linear RGB\n");
    }
    PrepareVideoWindow();
    const HRESULT hr = renderer.initialize(hwnd, video_settings.adapter);
    if (FAILED(hr)) {
        char message[128];
        sprintf_s(message, "D3D11 initialization failed (0x%08lX).",
                  static_cast<unsigned long>(hr));
        MessageBoxA(hwnd, message, "rxpce", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    LogDumpDetail("D3D11 initialized, adapter=%d zoom=%d\n", video_settings.adapter,
                  video_settings.zoom);
    return TRUE;
}

void PrepareVideoWindow() {
    zoom = video_settings.zoom;
    ResizeVideoWindow(320, 240);
}

void ShutdownVideo() {
    renderer.shutdown();
    frame.clear();
}

BOOL ResetVideoToIdle() {
    ShutdownVideo();
    InvalidateRect(hwnd, nullptr, TRUE);
    UpdateWindow(hwnd);
    if (!InitializeVideo())
        return FALSE;
    frame.assign(320 * 240, 0xff000000u);
    return SUCCEEDED(renderer.render(frame.data(), 320, 240, video_settings.zoom));
}

bool SetFullscreenCursor(int hit_test, bool dialog_open) {
    if (!bFullScreen || !bActiveApp || dialog_open || hit_test != HTCLIENT)
        return false;
    SetCursor(nullptr);
    return true;
}

void PresentVideo(const rxpce_video::Pixel* pixels, int stride, int rows, int x, int y, int width,
                  int height, bool nonblocking) {
    if (!rxpce_video::convert_frame(pixels, stride, rows, x, y, width, height, palette, frame))
        return;
    if (last_width != width || last_height != height) {
        last_width = width;
        last_height = height;
        PostMessage(hwnd, WM_VIDEO_SIZE, width, height);
    }
    DisplayInfo(frame.data(), width, height);
    if (device_lost) {
        if (FAILED(renderer.initialize(hwnd, video_settings.adapter)))
            return;
        device_lost = false;
    }
    const HRESULT hr = renderer.render(frame.data(), width, height, bFullScreen ? 0 : zoom.load(),
                                       true, nonblocking);
    if (FAILED(hr) && hr != last_error)
        LogDumpDetail("D3D11 render failed: %08lX\n", static_cast<unsigned long>(hr));
    last_error = hr;
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        LogDumpDetail("D3D11 device lost, retrying next frame: %08lX\n",
                      static_cast<unsigned long>(hr));
        device_lost = true;
    }
}

void ResizeVideoWindow(int width, int height) {
    if (width == 0 && height == 0) {
        width = source_width;
        height = source_height;
    }
    if (width < 1 || width > 512 || height < 1 || height > 256)
        return;
    source_width = width;
    source_height = height;
    if (bFullScreen) {
        MONITORINFO monitor = {sizeof(monitor)};
        if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor)) {
            const auto& r = monitor.rcMonitor;
            SetWindowPos(hwnd, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return;
    }
    const int canvas_width = width < 320 ? 320 : width;
    const int canvas_height = height < 240 ? 240 : height;
    RECT client = {0, 0, canvas_width * zoom.load(), canvas_height * zoom.load()};
    RECT current;
    if (GetClientRect(hwnd, &current) && current.right == client.right &&
        current.bottom == client.bottom)
        return;
    AdjustWindowRectExForDpi(
        &client, static_cast<DWORD>(GetWindowLongPtr(hwnd, GWL_STYLE)), GetMenu(hwnd) != nullptr,
        static_cast<DWORD>(GetWindowLongPtr(hwnd, GWL_EXSTYLE)), GetDpiForWindow(hwnd));
    SetWindowPos(hwnd, nullptr, 0, 0, client.right - client.left, client.bottom - client.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void ZoomWindow() {
    if (bFullScreen)
        return;
    SetVideoZoom(video_settings.zoom % 4 + 1);
    char text[32];
    sprintf_s(text, "Window zoom: %dx", video_settings.zoom);
    SetInfoString(text);
}

void SetVideoZoom(int scale) {
    if (scale < 1 || scale > 4)
        return;
    video_settings.zoom = scale;
    zoom = scale;
    ResizeVideoWindow(0, 0);
}

void ChangeWindowMode() {
    if (!bFullScreen) {
        GetWindowRect(hwnd, &saved_window);
        saved_style = GetWindowLongPtr(hwnd, GWL_STYLE);
        saved_exstyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        saved_menu = GetMenu(hwnd);
        MONITORINFO monitor = {sizeof(monitor)};
        if (!GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor))
            return;
        bFullScreen = true;
        SetMenu(hwnd, nullptr);
        SetWindowLongPtr(hwnd, GWL_STYLE, WS_POPUP | (saved_style & WS_VISIBLE));
        SetWindowLongPtr(hwnd, GWL_EXSTYLE,
                         saved_exstyle & ~(WS_EX_CLIENTEDGE | WS_EX_WINDOWEDGE |
                                           WS_EX_DLGMODALFRAME | WS_EX_STATICEDGE));
        const auto& r = monitor.rcMonitor;
        SetWindowPos(hwnd, HWND_TOP, r.left, r.top, r.right - r.left, r.bottom - r.top,
                     SWP_FRAMECHANGED | SWP_NOACTIVATE);
    } else {
        bFullScreen = false;
        SetWindowLongPtr(hwnd, GWL_STYLE, saved_style);
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, saved_exstyle);
        SetMenu(hwnd, saved_menu);
        SetWindowPos(hwnd, nullptr, saved_window.left, saved_window.top,
                     saved_window.right - saved_window.left, saved_window.bottom - saved_window.top,
                     SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE);
        ResizeVideoWindow(source_width, source_height);
    }
}
