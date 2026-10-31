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
rxpce_video::PresentationFrame frame;
std::unique_ptr<rxpce_video::FramePresenter> presenter;
struct PresentationOptions {
    int zoom = 1;
    rxpce_video::AspectMode mode = rxpce_video::AspectMode::square_pixels;
};
std::atomic<PresentationOptions> presentation{PresentationOptions{}};
int source_width = 320, source_height = 240; // UI thread only.
RECT saved_window = {};
LONG_PTR saved_style = 0;
LONG_PTR saved_exstyle = 0;
HMENU saved_menu = nullptr;
int last_width = 0, last_height = 0; // Emulation thread only.
bool device_lost = false;            // Presentation worker only while active.
HRESULT last_error = S_OK;
} // namespace

BOOL InitializeVideo() {
    presenter.reset();
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
    const int adapter = video_settings.adapter;
    try {
        presenter = std::make_unique<rxpce_video::FramePresenter>(
            [adapter](const rxpce_video::PresentationFrame& image) {
                if (device_lost) {
                    if (FAILED(renderer.initialize(hwnd, adapter)))
                        return;
                    device_lost = false;
                }
                const HRESULT status =
                    renderer.render(image.pixels.data(), image.width, image.height, image.scale,
                                    true, image.nonblocking, image.aspect_mode);
                if (FAILED(status) && status != last_error)
                    LogDumpDetail("D3D11 render failed: %08lX\n",
                                  static_cast<unsigned long>(status));
                last_error = status;
                if (status == DXGI_ERROR_DEVICE_REMOVED || status == DXGI_ERROR_DEVICE_RESET) {
                    LogDumpDetail("D3D11 device lost, retrying next frame: %08lX\n",
                                  static_cast<unsigned long>(status));
                    device_lost = true;
                }
            });
    } catch (const std::exception& error) {
        LogDumpDetail("D3D11 presentation worker failed: %s\n", error.what());
        renderer.shutdown();
        return FALSE;
    }
    LogDumpDetail("D3D11 initialized, adapter=%d zoom=%d\n", video_settings.adapter,
                  video_settings.zoom);
    return TRUE;
}

void PrepareVideoWindow() {
    presentation.store({video_settings.zoom, video_settings.aspect_mode});
    ResizeVideoWindow(320, 240);
}

void ShutdownVideo() {
    // Emulation has already joined. Drain synchronous window messages while joining D3D.
    presenter.reset();
    renderer.shutdown();
    frame = {};
}

rxpce_video::PresentationStatistics TakeVideoPresentationStatistics() {
    return presenter ? presenter->take_statistics() : rxpce_video::PresentationStatistics{};
}

BOOL ResetVideoToIdle() {
    ShutdownVideo();
    InvalidateRect(hwnd, nullptr, TRUE);
    UpdateWindow(hwnd);
    if (!InitializeVideo())
        return FALSE;
    frame.pixels.assign(320 * 240, 0xff000000u);
    frame.width = 320;
    frame.height = 240;
    frame.scale = video_settings.zoom;
    frame.aspect_mode = video_settings.aspect_mode;
    return presenter->submit(frame) ? TRUE : FALSE;
}

bool SetFullscreenCursor(int hit_test, bool dialog_open) {
    if (!bFullScreen || !bActiveApp || dialog_open || hit_test != HTCLIENT)
        return false;
    SetCursor(nullptr);
    return true;
}

void PresentVideo(const rxpce_video::Pixel* pixels, int stride, int rows, int x, int y, int width,
                  int height, bool nonblocking) {
    if (!rxpce_video::convert_frame(pixels, stride, rows, x, y, width, height, palette,
                                    frame.pixels))
        return;
    if (last_width != width || last_height != height) {
        last_width = width;
        last_height = height;
        PostMessage(hwnd, WM_VIDEO_SIZE, width, height);
    }
    DisplayInfo(frame.pixels.data(), width, height);
    const auto options = presentation.load();
    frame.width = width;
    frame.height = height;
    frame.scale = bFullScreen ? 0 : options.zoom;
    frame.aspect_mode = options.mode;
    frame.nonblocking = nonblocking;
    if (presenter)
        presenter->submit(frame);
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
    const auto options = presentation.load();
    const auto canvas = rxpce_video::window_size(width, height, options.zoom, options.mode);
    if (canvas.width == 0 || canvas.height == 0)
        return;
    RECT client = {0, 0, canvas.width, canvas.height};
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
    SetVideoPresentation(video_settings.aspect_mode, scale);
}
void SetVideoPresentation(rxpce_video::AspectMode mode, int scale) {
    if (scale < 1 || scale > 4 || !rxpce_video::valid_aspect_mode(mode))
        return;
    video_settings.zoom = scale;
    video_settings.aspect_mode = mode;
    presentation.store({scale, mode});
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
