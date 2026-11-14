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
#include "log.h"
#include <mmsystem.h>
#include <vector>
#include <new>

namespace rxpce_host {
RuntimeSettings runtime;
namespace {
std::vector<rxpce_video::Pixel> pixels;
DWORD fps_start = 0;
unsigned fps_frames = 0;
int current_fps = 0;
}
rxpce_machine::FrameSurface HostFrame() {
    return {pixels.data(), pixels.size()};
}
bool InitializeHost(rxpce_machine::Machine& machine,
                    const std::function<void(rxpce_audio::OutputBackend)>& audio_unavailable) {
    InitializeInput();
    TRACE("Initializing Hardware\n");
    try {
        pixels.assign(rxpce_machine::frame_width * rxpce_machine::frame_height, 0);
    } catch (const std::bad_alloc&) {
        return false;
    }
    InitializeFrameTiming(rxpce_video::vce_line_ticks * 262, rxpce_video::video_master_hz);
    auto sound = rxpce_host::runtime.sound;
    sound.base_clock_hz = machine.cpu_clock_hz;
    const bool sound_ready = InitSound(sound, hwnd, rxpce_audio::snapshot_psg(machine.io));
    InitDInput();
    InitInfo();
    fps_start = timeGetTime();
    fps_frames = 0;
    current_fps = 0;
    if (!sound_ready && sound.enabled) {
        LogDump("Audio output initialization failed: backend=%s; continuing without audio output\n",
                sound.backend == rxpce_audio::OutputBackend::wasapi ? "WASAPI" : "DirectSound");
        if (audio_unavailable)
            audio_unavailable(sound.backend);
    }
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
    const auto time = timeGetTime();
    ++fps_frames;
    const auto span = time - fps_start;
    if (span > 500) {
        current_fps = static_cast<int>((std::uint64_t(fps_frames) * 1000 + span / 2) / span);
        fps_frames = 0;
        fps_start = time;
    }
    if (rxpce_host::runtime.show_fps)
        SetInfoFps(current_fps);
    PresentVideo(frame.pixels, rxpce_machine::frame_width, rxpce_machine::frame_height, area.x,
                 area.y, area.width, area.height, !rxpce_host::runtime.auto_frame_skip.load());
}
} // namespace rxpce_host
