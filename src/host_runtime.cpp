#include "win32_support.h"
#include "host_runtime.h"
#include "host_settings.h"
#include "host_input.h"
#include "machine.h"
#include "psg_snapshot.h"
#include "Sound.h"
#include "di.h"
#include "info.h"
#include "video.h"
#include "frame_timing.h"
#include <mmsystem.h>
#include <vector>
#include <new>

namespace rxpce_host {
RuntimeSettings runtime;
namespace {
std::vector<rxpce_video::Pixel> pixels;
}
rxpce_machine::FrameSurface HostFrame() {
    return {pixels.data(), pixels.size()};
}
bool InitializeHost(rxpce_machine::Machine& machine) {
    InitializeInput();
    TRACE("Initializing Hardware\n");
    try {
        pixels.assign(rxpce_machine::frame_width * rxpce_machine::frame_height, 0);
    } catch (const std::bad_alloc&) {
        return false;
    }
    InitializeFrameTiming(rxpce_video::vce_line_ticks * 262, rxpce_video::video_master_hz);
    InitSound(rxpce_host::runtime.sound, hwnd, rxpce_audio::snapshot_psg(machine.io));
    InitDInput();
    InitInfo();
    return true;
}
void ShutdownHost() {
    TRACE("Shutting down...\n");
    std::vector<rxpce_video::Pixel>().swap(pixels);
    TrashSound();
    ReleaseDInput();
    TrashInfo();
    ShutdownFrameTiming();
}
void PresentFrame(rxpce_machine::FrameSurface frame, rxpce_machine::DisplayRectangle area) {
    if ((bFullScreen && !bActiveApp) || area.width == 0 || !frame.pixels ||
        frame.count < rxpce_machine::frame_width * rxpce_machine::frame_height)
        return;
    static DWORD old_time = 0;
    static unsigned frames = 0;
    static int fps = 0;
    const auto time = timeGetTime();
    ++frames;
    const auto span = time - old_time;
    if (span > 500) {
        fps = static_cast<int>((frames * 1000 + span / 2) / span);
        frames = 0;
        old_time = time;
    }
    if (rxpce_host::runtime.show_fps)
        SetInfoFps(fps);
    PresentVideo(frame.pixels, rxpce_machine::frame_width, rxpce_machine::frame_height, area.x,
                 area.y, area.width, area.height, !rxpce_host::runtime.auto_frame_skip.load());
}
} // namespace rxpce_host
