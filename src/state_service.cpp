#include "state_service.h"
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
    value.controllers = static_cast<unsigned>(rxpce_config::active_input.count);
    for (unsigned n = 0; n < 5; ++n)
        value.buttons[n] = static_cast<std::uint8_t>(rxpce_config::active_input.players[n].buttons);
    return value;
}
} // namespace
void SaveState() {
    auto& machine = CurrentMachine();
    const bool cd = IsCdActive();
    try {
        const auto binding = requirements(cd);
        rxpce_machine::AdvanceMachineVdc(machine, machine.cpu.cpu.elapsed);
        rxpce_audio::PsgPhaseState sound;
        if (cd) {
            SynchronizeCdState();
        }
        sound = CaptureSoundPhase(machine.cpu.cycles());
        const auto snapshot = rxpce_state::capture_machine(machine, binding.flags);
        snapshot->sound = sound;
        auto payload = cd ? rxpce_state::encode_machine(*snapshot, binding)
                          : rxpce_state::encode_hucard(*snapshot, binding);
        if (cd)
            AppendCdState(payload, sound);
        auto file = rxpce_state::header(payload.size(), binding.flags, binding.vdc_mode);
        file.insert(file.end(), payload.begin(), payload.end());
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
    auto& machine = CurrentMachine();
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
        std::unique_ptr<rxpce_state::MachineSnapshot> snapshot;
        std::string error;
        const bool decoded =
            cd ? rxpce_state::decode_machine(payload.data(), base, binding, snapshot, error)
               : rxpce_state::decode_hucard(payload.data(), total, binding, snapshot, error);
        if (!decoded) {
            LogDumpDetail("State rejected: %s\n", error.c_str());
            SetInfoString("Invalid state; see rxpce.log");
            return;
        }
        if (!ValidCartridgeBank(snapshot->bank)) {
            LogDumpDetail("State rejected: invalid cartridge bank\n");
            SetInfoString("Invalid cartridge bank");
            return;
        }
        std::unique_ptr<rxpce_cd::MachineState> saved_cd;
        if (cd) {
            saved_cd = PrepareCdState(payload.data() + base, total - base,
                                      snapshot->cpu.cpu.elapsed, error);
            if (!saved_cd || ((snapshot->io.irq_status & 1) != 0) != saved_cd->device.irq()) {
                LogDumpDetail("CD state rejected: %s\n",
                              saved_cd ? "IRQ2 mismatch" : error.c_str());
                SetInfoString("CD state rejected; see rxpce.log");
                return;
            }
        }
        rxpce_state::apply_machine(machine, *snapshot, binding.flags, rxpce_host::HostFrame());
        if (saved_cd)
            ApplyCdState(std::move(*saved_cd));
        RestoreCartridgeBank(snapshot->bank);
        const auto& sound = saved_cd ? saved_cd->sound : snapshot->sound;
        const bool sound_ok =
            RestoreSoundPhase(sound, machine.cpu.cycles(), rxpce_audio::snapshot_psg(machine.io));
        ResetFrameTiming();
        LogDumpDetail("State loaded successfully\n");
        SetInfoString(sound_ok ? "State loaded" : "State loaded; audio restart failed");
    } catch (const std::exception& error) {
        LogDumpDetail("State load failed: %s\n", error.what());
        SetInfoString("Can't load state file");
    }
}
