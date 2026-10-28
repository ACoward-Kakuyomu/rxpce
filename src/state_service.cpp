#include "state_service.h"
#include "file_paths.h"
#include "machine_runtime.h"
#include "host_runtime.h"
#include "host_settings.h"
#include "psg_snapshot.h"
using rxpce_host::CurrentMachine;
#include "machine_state_codec.h"
#include "state_format.h"
#include "cd_machine_state.h"
#include "cd_state_file.h"
#include "graphics_memory.h"
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
    auto path = rxpce_rom::state_path(CurrentMachine().source, rxpce_host::runtime.state_directory);
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
    value.flags = cd ? CdStateFlags() : CartridgeFlags();
    value.line_period = machine.cpu.line_period;
    value.timer_period = machine.cpu.timer_period;
    value.frame_lines = static_cast<unsigned>(machine.frame_lines);
    value.controllers = static_cast<unsigned>(rxpce_config::active_input.count);
    for (unsigned n = 0; n < 5; ++n)
        value.buttons[n] = static_cast<std::uint8_t>(rxpce_config::active_input.players[n].buttons);
    return value;
}
std::unique_ptr<rxpce_state::MachineSnapshot>
capture(const rxpce_state::SnapshotRequirements& binding) {
    auto& machine = CurrentMachine();
    auto value = std::make_unique<rxpce_state::MachineSnapshot>();
    std::copy_n(machine.ram.data(), value->ram.size(), value->ram.begin());
    std::copy_n(machine.graphics.vram(), value->vram.size(), value->vram.begin());
    std::copy_n(machine.video.sprites.data(), value->sprites.size(), value->sprites.begin());
    std::copy_n(machine.video.palette.data(), value->palette.size(), value->palette.begin());
    value->scanline = machine.scanline;
    value->io = machine.io;
    std::copy_n(machine.backup_ram.data(), value->backup.size(), value->backup.begin());
    value->bank = CartridgeBank();
    value->cpu = machine.cpu;
    if (binding.flags & 1)
        std::copy_n(machine.populous.data(), value->populous.size(), value->populous.begin());
    return value;
}
void apply(const rxpce_state::MachineSnapshot& value,
           const rxpce_state::SnapshotRequirements& binding) {
    auto& machine = CurrentMachine();
    std::copy(value.ram.begin(), value.ram.end(), machine.ram.data());
    std::copy(value.vram.begin(), value.vram.end(), machine.graphics.vram());
    std::copy(value.sprites.begin(), value.sprites.end(), machine.video.sprites.data());
    std::copy(value.palette.begin(), value.palette.end(), machine.video.palette.data());
    machine.scanline = value.scanline;
    machine.io = value.io;
    std::copy(value.backup.begin(), value.backup.end(), machine.backup_ram.data());
    const auto revision = machine.cpu.revision + 1;
    machine.cpu = value.cpu;
    machine.cpu.revision = revision;
    if (binding.flags & 1)
        std::copy(value.populous.begin(), value.populous.end(), machine.populous.data());
}
} // namespace
void SaveState() {
    auto& machine = CurrentMachine();
    const bool cd = IsCdActive();
    try {
        const auto binding = requirements(cd);
        rxpce_audio::PsgPhaseState sound;
        if (cd) {
            SynchronizeCdState();
            sound = CaptureSoundPhase(machine.cpu.cycles());
        }
        const auto snapshot = capture(binding);
        auto payload = rxpce_state::encode_machine(*snapshot, binding);
        if (cd)
            AppendCdState(payload, sound);
        auto file = rxpce_state::header(payload.size(), binding.flags);
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
        const auto total = base + (cd ? CdStateBytes() : 0);
        std::vector<char> payload;
        if (cd) {
            std::vector<char> file;
            std::string error;
            if (!rxpce_cd::read_state_file(CdStatePath(rxpce_host::runtime.state_directory),
                                           16 + total, file, error)) {
                LogDumpDetail("CD state rejected: %s\n", error.c_str());
                SetInfoString("Invalid or unreadable CD state file");
                return;
            }
            const auto expected = rxpce_state::header(total, binding.flags);
            if (!std::equal(expected.begin(), expected.end(), file.begin())) {
                LogDumpDetail("CD state rejected: header mismatch\n");
                SetInfoString("CD state format or configuration mismatch");
                return;
            }
            payload.assign(file.begin() + 16, file.end());
        } else {
            std::ifstream input(StateFilePath(), std::ios::binary);
            if (!rxpce_state::read(input, total, binding.flags, payload)) {
                LogDumpDetail("State rejected: header or length mismatch\n");
                SetInfoString("Invalid or unreadable state file");
                return;
            }
        }
        std::unique_ptr<rxpce_state::MachineSnapshot> snapshot;
        std::string error;
        if (!rxpce_state::decode_machine(payload.data(), base, binding, snapshot, error)) {
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
        apply(*snapshot, binding);
        if (saved_cd)
            ApplyCdState(std::move(*saved_cd));
        RestoreCartridgeBank(snapshot->bank);
        std::fill_n(machine.graphics.background_dirty(),
                    rxpce_machine::GraphicsMemory::background_tiles, 1);
        std::fill_n(machine.graphics.sprite_dirty(), rxpce_machine::GraphicsMemory::sprite_tiles,
                    1);
        const auto frame = rxpce_host::HostFrame();
        std::fill_n(frame.pixels, frame.count, machine.video.palette[0]);
        bool sound_ok = true;
        if (saved_cd)
            sound_ok = RestoreSoundPhase(saved_cd->sound, machine.cpu.cycles(),
                                         rxpce_audio::snapshot_psg(machine.io));
        else
            ResetSound(machine.cpu.cycles(), rxpce_audio::snapshot_psg(machine.io));
        ResetFrameTiming();
        LogDumpDetail("State loaded successfully\n");
        SetInfoString(sound_ok ? "State loaded" : "State loaded; audio restart failed");
    } catch (const std::exception& error) {
        LogDumpDetail("State load failed: %s\n", error.what());
        SetInfoString("Can't load state file");
    }
}
