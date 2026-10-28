#include "win32_support.h"
#include "host_runtime.h"
#include "host_settings.h"
#include "host_input.h"
#include "machine.h"
#include "machine_commands.h"
#include "pce.h"
#include "state_service.h"
#include "info.h"
#include "log.h"
#include <atomic>
#include <cstdio>

namespace {
std::atomic_bool stop_requested{false};
}
void RequestMachineStop() {
    stop_requested = true;
}
void ResetMachineStop() {
    stop_requested = false;
}
bool IsMachineStopRequested() {
    return stop_requested.load();
}
rxpce_host::CommandChannel& MachineCommands() {
    static rxpce_host::CommandChannel channel;
    return channel;
}

namespace rxpce_host {
bool PollHostCommands(rxpce_machine::Machine& machine) {
    if (IsMachineStopRequested())
        return false;
    PollInput(machine);
    const auto commands = MachineCommands().take();
    if (commands & EV_SAVE)
        SaveState();
    if (commands & EV_LOAD)
        LoadState();
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
    return !IsMachineStopRequested();
}
} // namespace rxpce_host
