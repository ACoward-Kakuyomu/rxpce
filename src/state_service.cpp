#include "state_service.h"
#include "replay_media.h"
#include "file_paths.h"
#include "machine_runtime.h"
#include "machine_vdc.h"
#include "host_runtime.h"
#include "host_settings.h"
#include "psg_snapshot.h"
using rxpce_host::CurrentMachine;
#include "machine_state_codec.h"
#include "state_format.h"
#include "cd_machine_state.h"
#include "cd_state_file.h"
#include "archive.h"
#include "nvram.h"
#include "pce.h"
#include "Sound.h"
#include "Info.h"
#include "log.h"
#include "settings.h"
#include "frame_timing.h"
#include <algorithm>
#include <fstream>
#include <utility>

bool IsCdActive();

std::filesystem::path StateFilePath() {
    auto path = rxpce_rom::state_path(CurrentMachine().source, rxpce_host::runtime.state_directory,
                                      CurrentMachine().vdc_mode);
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code error;
        std::filesystem::create_directories(parent, error);
        if (error)
            LogDump("Cannot create state folder (%d): %s\n", error.value(),
                    rxpce_paths::utf8(parent).c_str());
    }
    return path;
}
namespace {
rxpce_state::SnapshotRequirements requirements(bool cd) {
    auto& machine = CurrentMachine();
    rxpce_state::SnapshotRequirements value;
    value.vdc_mode = machine.vdc_mode;
    value.flags = cd ? CdStateFlags() : CartridgeFlags();
    value.line_period = machine.cpu.line_period;
    value.timer_period = machine.cpu.timer_period;
    value.cpu_clock_hz = machine.cpu_clock_hz;
    value.controllers = machine.io.controllers.count;
    value.buttons = machine.io.controllers.modes;
    return value;
}
void synchronize_snapshot(rxpce_machine::Machine& machine, bool cd) {
    rxpce_machine::AdvanceMachineVdc(machine, machine.cpu.cpu.elapsed);
    if (cd)
        SynchronizeCdState();
}
bool restore_payload(const char* payload, std::size_t base, std::size_t total,
                     const rxpce_state::SnapshotRequirements& binding, bool cd, bool& sound_ok,
                     std::string& error) {
    auto& machine = CurrentMachine();
    std::unique_ptr<rxpce_state::MachineSnapshot> snapshot;
    const bool decoded =
        cd ? rxpce_state::decode_machine(payload, base, binding, snapshot, error)
           : rxpce_state::decode_hucard(payload, total, binding, snapshot, error);
    if (!decoded)
        return false;
    if (!ValidCartridgeBank(snapshot->bank)) {
        error = "invalid cartridge bank";
        return false;
    }
    std::unique_ptr<rxpce_cd::MachineState> saved_cd;
    if (cd) {
        saved_cd = PrepareCdState(payload + base, total - base, snapshot->cpu.cpu.elapsed, error);
        if (!saved_cd)
            return false;
        if (((snapshot->io.irq_status & 1) != 0) != saved_cd->device.irq()) {
            error = "IRQ2 mismatch";
            return false;
        }
    }
    rxpce_state::apply_machine(machine, *snapshot, binding.flags, rxpce_host::HostFrame());
    if (saved_cd)
        ApplyCdState(std::move(*saved_cd));
    RestoreCartridgeBank(snapshot->bank);
    const auto& sound = saved_cd ? saved_cd->sound : snapshot->sound;
    sound_ok =
        RestoreSoundPhase(sound, machine.cpu.cycles(), rxpce_audio::snapshot_psg(machine.io));
    ResetFrameTiming();
    error.clear();
    return true;
}
} // namespace

std::vector<char> CaptureStateFile() {
    auto& machine = CurrentMachine();
    const bool cd = IsCdActive();
    const auto binding = requirements(cd);
    synchronize_snapshot(machine, cd);
    const auto sound = CaptureSoundPhase(machine.cpu.cycles());
    const auto snapshot = rxpce_state::capture_machine(machine, binding.flags);
    snapshot->sound = sound;
    auto payload = cd ? rxpce_state::encode_machine(*snapshot, binding)
                      : rxpce_state::encode_hucard(*snapshot, binding);
    if (cd)
        AppendCdState(payload, sound);
    auto file = rxpce_state::header(payload.size(), binding.flags, binding.vdc_mode);
    file.insert(file.end(), payload.begin(), payload.end());
    return file;
}

rxpce_replay::Digest CaptureReplayDigest() {
    auto& machine = CurrentMachine();
    const bool cd = IsCdActive();
    const auto binding = requirements(cd);
    synchronize_snapshot(machine, cd);
    auto snapshot = rxpce_state::capture_machine(machine, binding.flags);
    snapshot->cpu.Trace = false;
    snapshot->cpu.Trap = 0xffff;
    snapshot->palette.fill(0);
    auto bytes = rxpce_state::encode_machine(*snapshot, binding);
    if (cd) {
        const auto controller = machine.cd_controller.save_state(false);
        bytes.insert(bytes.end(), controller.begin(), controller.end());
        bytes.insert(bytes.end(), machine.cd_ram.begin(), machine.cd_ram.end());
        if (binding.flags == 0x200)
            bytes.insert(bytes.end(), machine.super_ram.begin(), machine.super_ram.end());
    }
    return rxpce_replay::hash_bytes(bytes.data(), bytes.size());
}

bool RestoreStateFile(const std::vector<char>& file, bool& sound_ok, std::string& error) {
    const bool cd = IsCdActive();
    const auto binding = requirements(cd);
    const auto base = rxpce_state::machine_state_bytes(binding.flags);
    const auto total = cd ? base + CdStateBytes() : rxpce_state::hucard_state_bytes(binding.flags);
    const auto prefix = rxpce_state::header(total, binding.flags, binding.vdc_mode);
    if (file.size() != prefix.size() + total ||
        !std::equal(prefix.begin(), prefix.end(), file.begin())) {
        error = "State header or length mismatch";
        return false;
    }
    return restore_payload(file.data() + prefix.size(), base, total, binding, cd, sound_ok, error);
}

void SaveState() {
    const bool cd = IsCdActive();
    try {
        const auto file = CaptureStateFile();
        std::string error;
        const bool ok =
            cd ? rxpce_cd::write_state_file(CdStatePath(rxpce_host::runtime.state_directory), file,
                                            error)
               : rxpce_storage::save_nvram(StateFilePath().native(),
                                           reinterpret_cast<const unsigned char*>(file.data()),
                                           file.size(), error);
        if (ok)
            LogDumpDetail(cd ? "CD state saved successfully\n" : "State saved successfully\n");
        else
            LogDumpDetail("State save failed: %s\n", error.c_str());
        SetInfoString(ok ? "State saved" : "Can't save state file");
    } catch (const std::exception& error) {
        LogDumpDetail("State save failed: %s\n", error.what());
        SetInfoString("Can't save state file");
    }
}
void LoadState() {
    const bool cd = IsCdActive();
    try {
        const auto binding = requirements(cd);
        const auto base = rxpce_state::machine_state_bytes(binding.flags);
        const auto total =
            cd ? base + CdStateBytes() : rxpce_state::hucard_state_bytes(binding.flags);
        std::vector<char> payload;
        if (cd) {
            std::string error;
            if (!rxpce_cd::read_state_payload(CdStatePath(rxpce_host::runtime.state_directory),
                                              total, binding.flags, payload, error,
                                              binding.vdc_mode)) {
                LogDumpDetail("CD state rejected: %s\n", error.c_str());
                SetInfoString("Invalid or unreadable CD state file");
                return;
            }
        } else {
            std::ifstream input(StateFilePath(), std::ios::binary);
            if (!rxpce_state::read(input, total, binding.flags, payload, binding.vdc_mode)) {
                LogDumpDetail("State rejected: header or length mismatch\n");
                SetInfoString("Invalid or unreadable state file");
                return;
            }
        }
        bool sound_ok = false;
        std::string error;
        if (!restore_payload(payload.data(), base, total, binding, cd, sound_ok, error)) {
            LogDumpDetail("State rejected: %s\n", error.c_str());
            SetInfoString("Invalid state; see rxpce.log");
            return;
        }
        LogDumpDetail("State loaded successfully\n");
        SetInfoString(sound_ok ? "State loaded" : "State loaded; audio restart failed");
    } catch (const std::exception& error) {
        LogDumpDetail("State load failed: %s\n", error.what());
        SetInfoString("Can't load state file");
    }
}
