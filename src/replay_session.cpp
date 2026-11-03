#include "replay_session.h"
#include "host_input.h"
#include "info.h"
#include "log.h"
#include "machine.h"
#include "replay_media.h"
#include "state_service.h"
#include <stdexcept>

namespace rxpce_replay {
namespace {
std::uint64_t time_at_boundary(const rxpce_machine::Machine& machine) {
    rxpce_video::CpuVideoClock clock(machine.cpu_clock_hz);
    clock.advance_to(machine.cpu.cpu.elapsed);
    return clock.state().master_time;
}
MediaIdentity current_media(const rxpce_machine::Machine& machine) {
    if (machine.cd_session) {
        const auto& cd = *machine.cd_session;
        return identify_cd(cd.source, cd.disc.identity(), cd.bios.profile.rom_sha256);
    }
    const auto& rom = machine.rom_bytes();
    return identify_hucard(rom.data(), rom.size());
}
} // namespace
bool Session::begin_playback(rxpce_machine::Machine& machine) {
    if (!source_)
        return true;
    std::string error;
    bool sound_ok = false;
    if (!matches_media(*source_, current_media(machine), error) ||
        !RestoreStateFile(source_->state, sound_ok, error)) {
        LogDump("Replay rejected: %s\n", error.c_str());
        SetInfoString(error.c_str());
        return false;
    }
    playback_.emplace(*source_);
    SetInfoString(sound_ok ? "Replay playback" : "Replay playback; audio restart failed");
    return play_input(machine);
}
bool Session::play_input(rxpce_machine::Machine& machine) {
    std::string error;
    const auto result = playback_->step(time_at_boundary(machine), machine.io.controllers.buttons,
                                        CaptureReplayDigest, error);
    if (result == PlaybackResult::applied)
        return true;
    if (result == PlaybackResult::mismatch) {
        LogDump("%s\n", error.c_str());
        SetInfoString(error.c_str());
    } else {
        LogDumpDetail("Replay completed\n");
        SetInfoString("Replay completed");
    }
    return false;
}
void Session::finish(rxpce_machine::Machine& machine) {
    finish_recording(draft_, time_at_boundary(machine), CaptureReplayDigest);
    {
        std::lock_guard<std::mutex> lock(finished_mutex_);
        finished_.emplace(std::move(draft_));
    }
    recording_ = false;
    LogDumpDetail("Replay recording finished\n");
    if (notify_finished_)
        notify_finished_();
    SetInfoString("Replay recording finished");
}
std::optional<Recording> Session::take_finished() {
    std::lock_guard<std::mutex> lock(finished_mutex_);
    auto value = std::move(finished_);
    finished_.reset();
    return value;
}
bool Session::boundary(rxpce_machine::Machine& machine, bool toggle, bool stopping) {
    if (playback_)
        return !stopping && play_input(machine);
    if (recording() && (toggle || stopping || draft_.inputs.size() == max_inputs))
        finish(machine);
    else if (toggle && !stopping) {
        {
            std::lock_guard<std::mutex> lock(finished_mutex_);
            if (finished_) {
                SetInfoString("Save or discard the previous replay first");
                toggle = false;
            }
        }
        if (toggle) {
            const auto identity = current_media(machine);
            const auto& pads = machine.io.controllers;
            draft_ = Recording{};
            draft_.kind = identity.kind;
            draft_.media = identity.media;
            draft_.bios = identity.bios;
            draft_.mode = machine.vdc_mode;
            draft_.clock_hz = machine.cpu_clock_hz;
            draft_.players = pads.count;
            draft_.pad_modes = pads.modes;
            draft_.state = CaptureStateFile();
            recording_ = true;
            LogDumpDetail("Replay recording started\n");
            SetInfoString("Replay recording");
        }
    }
    if (stopping)
        return false;
    if (recording()) {
        append_input(draft_, time_at_boundary(machine), [&] {
            rxpce_host::PollInput(machine);
            return machine.io.controllers.buttons;
        }, CaptureReplayDigest);
    } else {
        rxpce_host::PollInput(machine);
    }
    return true;
}
} // namespace rxpce_replay
