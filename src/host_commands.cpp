#include "win32_support.h"
#include "host_runtime.h"
#include "host_settings.h"
#include "host_input.h"
#include "machine.h"
#include "machine_commands.h"
#include "pce.h"
#include "state_service.h"
#include "replay_session.h"
#include "cheats_session.h"
#include "memory_search_pause.h"
#include "frame_timing.h"
#include "video.h"
#include "info.h"
#include "log.h"
#include <atomic>
#include <cstdio>
#ifdef SOUND
#include "Sound.h"
#endif

namespace {
std::atomic_bool stop_requested{false};
}
void RequestMachineStop() {
    stop_requested = true;
    rxpce_search::pause_channel().cancel();
}
void ResetMachineStop() {
    stop_requested = false;
    rxpce_search::pause_channel().reset();
}
bool IsMachineStopRequested() {
    return stop_requested.load();
}
rxpce_host::CommandChannel& MachineCommands() {
    static rxpce_host::CommandChannel channel;
    return channel;
}

namespace rxpce_host {
bool PollHostCommands(rxpce_machine::Machine& machine, rxpce_replay::Session& replay,
                      rxpce_cheats::Session& cheats) {
    auto commands = MachineCommands().take();
    if ((commands & EV_REPLAY_RECORD) && cheats.freezes()) {
        commands &= ~EV_REPLAY_RECORD;
        SetInfoString("Disable Always Apply cheats before recording a replay");
        LogDumpDetail("Replay recording rejected: Always Apply cheats are enabled\n");
    }
    cheats.boundary(IsMachineStopRequested() || (commands & (EV_LOAD | EV_RESET)) != 0);
    const bool was_recording = replay.recording();
    if (!replay.boundary(machine, (commands & EV_REPLAY_RECORD) != 0, IsMachineStopRequested()))
        return false;
    if (was_recording || replay.recording() || replay.source())
        commands &= ~(EV_SAVE | EV_LOAD | EV_RESET);
    if (commands & EV_SAVE)
        SaveState();
    if (commands & EV_LOAD)
        LoadState();
    if (commands & (EV_LOAD | EV_RESET))
        rxpce_search::pause_channel().invalidate();
    bool changed = false;
    if (commands & EV_DOWNSKIPRATE) {
        if (rxpce_host::runtime.frame_skip > 0)
            --rxpce_host::runtime.frame_skip;
        changed = true;
    }
    if (commands & EV_UPSKIPRATE) {
        if (rxpce_host::runtime.frame_skip < 15)
            ++rxpce_host::runtime.frame_skip;
        changed = true;
    }
    if (commands & EV_AUTOFRAMESKIP) {
        rxpce_host::runtime.auto_frame_skip = !rxpce_host::runtime.auto_frame_skip;
#ifdef SOUND
        DiscardSoundOutput(machine.cpu.cycles());
#endif
        changed = true;
    }
    if (changed) {
        char text[32];
        std::snprintf(text, sizeof(text),
                      rxpce_host::runtime.auto_frame_skip ? "FrameSkip:%d(auto)" : "FrameSkip:%d",
                      rxpce_host::runtime.frame_skip.load());
        SetInfoString(text);
        LogDumpDetail("%s\n", text);
    }
    if (commands & EV_RESET) {
        ResetInput();
        ResetPCE(&machine.cpu);
    }
    const bool can_search = !was_recording && !replay.recording() && !replay.source();
    if ((commands & EV_MEMORY_SEARCH) && !can_search)
        PostMessage(hwnd, WM_APP + 10, 0, 0);
    if ((commands & EV_MEMORY_SEARCH) && can_search &&
        !IsMachineStopRequested()) {
        auto context = cheats.capture(machine);
#ifdef SOUND
        SetSoundPaused(true, machine.cpu.cycles());
#endif
        rxpce_search::pause_channel().wait(std::move(context), [] {
            return PostMessage(hwnd, WM_APP + 10, 0, 0) != FALSE;
        }, [&](const rxpce_search::Pause::Request& request) {
            return cheats.process(machine, request);
        }, [] {
            return PostMessage(hwnd, WM_APP + 12, 0, 0) != FALSE;
        });
#ifdef SOUND
        SetSoundPaused(false, machine.cpu.cycles());
#endif
        ResetInput();
        ResetFrameTiming();
    }
    return !IsMachineStopRequested();
}
} // namespace rxpce_host
