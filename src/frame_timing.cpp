#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "frame_scheduler.h"
#include "frame_timing.h"
#include "log.h"
#include "unique_handle.h"
#include "multimedia_task.h"
#include <chrono>

namespace {
rxpce_timing::FrameScheduler scheduler;
rxpce_host::UniqueHandle timer;
std::int64_t sample_start = 0;
unsigned frames = 0, render_slots = 0;
int sample_skip = -1;
bool sample_auto = false;
std::int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
} // namespace

void ResetFrameTiming() {
    scheduler.reset();
    sample_start = 0;
    frames = render_slots = 0;
    sample_skip = -1;
}
void ShutdownFrameTiming() {
    if (timer) {
        CancelWaitableTimer(timer.get());
        timer.reset();
    }
    ResetFrameTiming();
}
void InitializeFrameTiming(unsigned frame_cycles, unsigned clock_hz) {
    ShutdownFrameTiming();
    scheduler.set_frame_clock(frame_cycles, clock_hz);
    LogDumpDetail("Frame clock: cycles=%u base=%u\n", frame_cycles, clock_hz);
    timer.reset(CreateWaitableTimerEx(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                      TIMER_MODIFY_STATE | SYNCHRONIZE));
    if (!timer)
        timer.reset(CreateWaitableTimer(nullptr, FALSE, nullptr));
}
bool BeginVideoFrame(int minimum_skip, bool automatic, std::uint64_t completed_master_time) {
    thread_local rxpce_host::MultimediaTask task(L"Games");
    if (task.set_enabled(automatic))
        LogDumpDetail("Producer multimedia scheduling: auto=%d mmcss=%d\n", automatic ? 1 : 0,
                      task.active() ? 1 : 0);
    auto now = now_ns();
    for (auto delay = scheduler.wait_ns(now, automatic, completed_master_time); delay > 0;
         delay = scheduler.wait_ns(now, automatic, completed_master_time)) {
        LARGE_INTEGER due;
        due.QuadPart = -((delay + 99) / 100);
        if (!timer || !SetWaitableTimer(timer.get(), &due, 0, nullptr, nullptr, FALSE) ||
            WaitForSingleObject(timer.get(), INFINITE) != WAIT_OBJECT_0)
            Sleep(static_cast<DWORD>((delay + 999999) / 1000000));
        now = now_ns();
    }
    const bool draw = scheduler.begin(now, minimum_skip, automatic, completed_master_time);
    if (sample_skip != minimum_skip || sample_auto != automatic) {
        sample_start = 0;
        frames = render_slots = 0;
        sample_skip = minimum_skip;
        sample_auto = automatic;
    }
    if (!sample_start) {
        sample_start = now;
        return draw;
    }
    ++frames;
    if (draw)
        ++render_slots;
    if (now - sample_start >= 2000000000) {
        LogDumpDetail("Frame timing: frames=%u render_slots=%u interval_ms=%lld auto=%d skip=%d\n",
                      frames, render_slots, (now - sample_start) / 1000000, automatic ? 1 : 0,
                      minimum_skip);
        sample_start = now;
        frames = render_slots = 0;
    }
    return draw;
}
