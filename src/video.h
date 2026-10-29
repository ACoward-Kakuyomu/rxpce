#pragma once
#include <windows.h>
#include <atomic>
#include <string>
#include <filesystem>
#include "video_frame.h"

constexpr UINT WM_VIDEO_INITIALIZE = WM_APP + 4;
constexpr UINT WM_VIDEO_SIZE = WM_APP + 7;
extern HWND hwnd;
extern std::atomic_bool bActiveApp, bFullScreen;
struct VideoSettings {
    int adapter = -1;
    int zoom = 1;
    rxpce_video::AspectMode aspect_mode = rxpce_video::AspectMode::square_pixels;
    std::filesystem::path palette_file;
};
extern VideoSettings video_settings; // UI thread only, except during startup/shutdown.
BOOL InitializeVideo();
void PrepareVideoWindow();
void ShutdownVideo();
BOOL ResetVideoToIdle();
bool SetFullscreenCursor(int hit_test, bool dialog_open);
void PresentVideo(const rxpce_video::Pixel* pixels, int stride, int rows, int x, int y, int width,
                  int height, bool nonblocking);
void ChangeWindowMode();
void ZoomWindow();
void SetVideoZoom(int zoom);
void SetVideoPresentation(rxpce_video::AspectMode mode, int zoom);
void ResizeVideoWindow(int width, int height);
