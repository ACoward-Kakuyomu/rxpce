/*
	Portable PC-Engine Emulator
	1998 by BERO bero@geocities.co.jp

    Modified 1998 by hmmx hmmx@geocities.co.jp
*/

#include "win32_support.h"
#include <stdlib.h>
#include "pce.h"
#include <algorithm>
#include "settings.h"
#include "frame_timing.h"
#include "archive.h"
#include "vram_dma.h"
#include "graphics_memory.h"
#include "machine_runtime.h"
#include "host_runtime.h"
#include "host_settings.h"
#include "psg_snapshot.h"
using rxpce_machine::Machine;
using rxpce_host::CurrentMachine;
#include "machine_devices.h"
#include "vdc_registers.h"
using rxpce_machine::vdc::IRQ2;
#include "state_service.h"
#include "file_paths.h"
#include "nvram.h"
#include "cd_session.h"
#include "cd_controller.h"
#include "cd_machine_state.h"
#include "cd_state_file.h"
#include "Debug.h"
#ifdef SOUND
#include "sound.h"
#endif // SOUND
#include "info.h"
#include "log.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

static std::atomic_bool cd_running{false};
bool IsCdActive() {
    return cd_running.load();
}
std::uint32_t CartridgeFlags() {
    return CurrentMachine().mapper().flags();
}
std::uint8_t CartridgeBank() {
    return CurrentMachine().mapper().selection();
}
bool ValidCartridgeBank(unsigned bank) {
    return CurrentMachine().mapper().valid_selection(bank);
}
void RestoreCartridgeBank(unsigned bank) {
    if (!CurrentMachine().restore_bank(bank))
        throw std::runtime_error("Invalid cartridge bank state");
}
using rxpce_host::HostFrame;

namespace {
rxpce_machine::DeviceHost& NativeDevices() {
    static rxpce_machine::DeviceHost ports = [] {
        rxpce_machine::DeviceHost value;
        value.frame = [](void*) { return HostFrame(); };
#ifdef SOUND
        value.psg_changed = [](void* context) {
            auto& machine = *static_cast<Machine*>(context);
            SoundPsgChanged(machine.cpu.cycles(), rxpce_audio::snapshot_psg(machine.io));
        };
        value.sync_audio = [](void*, std::uint32_t cycles) { SyncSoundCycles(cycles); };
#endif
        value.begin_frame = [](void*) {
            return BeginVideoFrame(rxpce_host::runtime.frame_skip,
                                   rxpce_host::runtime.auto_frame_skip.load());
        };
        value.poll_commands = [](void* context) {
            return rxpce_host::PollHostCommands(*static_cast<Machine*>(context));
        };
        value.present = [](void*, rxpce_machine::FrameSurface frame,
                           rxpce_machine::DisplayRectangle area) {
            rxpce_host::PresentFrame(frame, area);
        };
        value.controller_read = [](void* context, std::uint8_t input) {
            auto& machine = *static_cast<Machine*>(context);
            const auto& controller = machine.io.controllers;
            if (machine.input_read_log < 16 && controller.port < 5 &&
                controller.buttons[controller.port] && !controller.clr) {
                LogDumpDetail("Input I/O player=%u sel=%u bank=%u value=%x\n", controller.port + 1,
                              controller.sel, controller.phase[controller.port], input);
                ++machine.input_read_log;
            }
        };
#ifdef _DEBUG
        value.trace = [](void*, const char* format, std::va_list args) {
            char buffer[2048];
            vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
            OutputDebugStringA(buffer);
        };
#endif
        return value;
    }();
    return ports;
}
void ConnectMachine(Machine& machine) {
    NativeDevices().context = &machine;
    machine.connections.context = &machine;
    machine.connections.read_io = [](void* context, std::uint16_t address) {
        return rxpce_machine::ReadMachineIo(*static_cast<Machine*>(context), NativeDevices(),
                                            address);
    };
    machine.connections.write_io = [](void* context, std::uint16_t address, std::uint8_t value) {
        rxpce_machine::WriteMachineIo(*static_cast<Machine*>(context), NativeDevices(), address,
                                      value);
    };
    machine.connections.access = [](void* context, rxpce_machine::Region region,
                                    std::uint32_t address, bool write) {
        auto& state = *static_cast<Machine*>(context);
        using rxpce_machine::Region;
        if (region != Region::cd_ram && region != Region::super_ram)
            return;
        if (write) {
            if (!state.cd_write_logged) {
                state.cd_write_logged = true;
                LogDumpDetail("CD RAM written: physical=%06x\n", address);
            }
        } else if (!state.cd_execution_logged &&
                   address == ((std::uint32_t(state.cpu.cpu.mpr[state.cpu.cpu.pc >> 13]) << 13) |
                               (state.cpu.cpu.pc & 8191))) {
            state.cd_execution_logged = true;
            LogDumpDetail("CD program execution: PC=%04x physical=%06x\n", state.cpu.cpu.pc,
                          address);
        }
    };
    machine.connections.bank_changed = [](void* context, std::uint8_t bank) {
        auto& state = *static_cast<Machine*>(context);
        if (state.bank_log_count++ < 16)
            LogDumpDetail("Cartridge bank selected: %u\n", bank);
    };
}
} // namespace
std::size_t CdStateBytes() {
    auto& machine = CurrentMachine();
    return rxpce_cd::binding_bytes + rxpce_cd::Controller::state_bytes() +
           (machine.cd_ram.size() * sizeof(machine.cd_ram[0])) +
           (machine.configuration.kind == rxpce_machine::Kind::super_cdrom
                ? (machine.super_ram.size() * sizeof(machine.super_ram[0]))
                : 0) +
           rxpce_audio::phase_state_bytes;
}
std::uint32_t CdStateFlags() {
    auto& machine = CurrentMachine();
    return machine.configuration.kind == rxpce_machine::Kind::super_cdrom ? 0x200 : 0x100;
}
std::wstring CdStatePath(const std::filesystem::path& folder) {
    auto& machine = CurrentMachine();
    return rxpce_cd::state_path(machine.cd_session->source, folder.native());
}
static rxpce_cd::StateBinding CurrentCdBinding() {
    auto& machine = CurrentMachine();
    return {static_cast<std::uint32_t>(machine.configuration.kind),
            static_cast<std::uint32_t>(rxpce_host::runtime.base_clock_hz),
            machine.cd_session->bios.profile.rom_sha256, machine.cd_session->disc.identity()};
}
void SynchronizeCdState() {
    auto& machine = CurrentMachine();
    machine.cd_controller.advance(machine.cpu.cpu.elapsed);
    machine.io.irq_status =
        (machine.io.irq_status & ~IRQ2) | (machine.cd_controller.irq() ? IRQ2 : 0);
}
void AppendCdState(std::vector<char>& payload, const rxpce_audio::PsgPhaseState& sound) {
    auto& machine = CurrentMachine();
    rxpce_state::Writer w;
    rxpce_cd::write_binding(w, CurrentCdBinding());
    const auto device = machine.cd_controller.save_state();
    w.block(device.data(), device.size());
    w.block(machine.cd_ram.data(), (machine.cd_ram.size() * sizeof(machine.cd_ram[0])));
    if (machine.configuration.kind == rxpce_machine::Kind::super_cdrom)
        w.block(machine.super_ram.data(),
                (machine.super_ram.size() * sizeof(machine.super_ram[0])));
    const auto audio = rxpce_audio::encode_phase(sound);
    w.block(audio.data(), audio.size());
    payload.insert(payload.end(), w.data.begin(), w.data.end());
}
std::unique_ptr<rxpce_cd::MachineState> PrepareCdState(const char* data, std::size_t size,
                                                       std::uint64_t time, std::string& error) {
    auto& machine = CurrentMachine();
    try {
        if (size != CdStateBytes())
            throw std::runtime_error("Invalid CD state payload size");
        rxpce_state::Reader r(data, size);
        if (!rxpce_cd::read_binding(r, CurrentCdBinding(), error))
            return {};
        auto next = std::make_unique<rxpce_cd::MachineState>();
        next->device = machine.cd_controller;
        if (!next->device.load_state(r.cursor, rxpce_cd::Controller::state_bytes(), time, error))
            return {};
        r.cursor += rxpce_cd::Controller::state_bytes();
        r.remaining -= rxpce_cd::Controller::state_bytes();
        r.block(next->ram.data(), next->ram.size());
        if (machine.configuration.kind == rxpce_machine::Kind::super_cdrom)
            r.block(next->super_ram.data(), next->super_ram.size());
        if (!rxpce_audio::decode_phase(r.cursor, r.remaining, next->sound))
            throw std::runtime_error("Invalid PSG phase state");
        return next;
    } catch (const std::exception& ex) {
        error = ex.what();
        return {};
    }
}
void ApplyCdState(rxpce_cd::MachineState&& state) {
    auto& machine = CurrentMachine();
    machine.cd_controller = std::move(state.device);
    std::copy(state.ram.begin(), state.ram.end(), machine.cd_ram.data());
    if (machine.configuration.kind == rxpce_machine::Kind::super_cdrom)
        std::copy(state.super_ram.begin(), state.super_ram.end(), machine.super_ram.data());
    machine.cd_execution_logged = machine.cd_write_logged = false;
}
int CartLoad() {
    auto& machine = CurrentMachine();
    machine.cd_session = rxpce_cd::take();
    machine.configuration.kind =
        machine.cd_session ? machine.cd_session->bios.profile.kind : rxpce_machine::Kind::hucard;
    cd_running = machine.cd_session != nullptr;
    machine.cd_execution_logged = machine.cd_write_logged = false;
    if (machine.cd_session) {
        machine.set_rom(std::move(machine.cd_session->bios.bytes), 0);
        machine.source = machine.cd_session->source;

        machine.cd_controller.connect(
            &machine.cd_session->disc.toc(),
            [&machine](std::int64_t lba, std::array<unsigned char, 2048>& bytes,
                       std::string& error) {
                return machine.cd_session->disc.read_data(lba, bytes, error);
            },
            [&machine](std::int64_t lba, std::array<std::int16_t, 1176>& pcm, std::string& error) {
                return machine.cd_session->disc.read_audio(lba, pcm, error);
            });
        machine.cd_controller.configure_audio(
            rxpce_host::runtime.sound.enabled ? rxpce_host::runtime.sound.sample_rate : 0);
#ifdef SOUND
        SetSoundExtraSource([&machine](float* out, std::size_t frames, unsigned channels) {
            machine.cd_controller.advance(machine.cpu.cpu.elapsed);
            machine.cd_controller.read_audio(out, frames, channels);
        });
#endif
        machine.cd_controller.trace = [](const std::string& text) {
            if (text.find("CD error:") == 0) {
                LogDump("%s\n", text.c_str());
                SetInfoString("CD transfer error; see rxpce.log");
            } else
                LogDumpDetail("%s\n", text.c_str());
        };
        const char* versions[] = {"unknown", "1.0", "2.0", "2.1", "3.0"};
        LogDumpDetail("CD mounted: BIOS=%s kind=%u tracks=%zu identity=%s\n",
                      versions[static_cast<unsigned>(machine.cd_session->bios.profile.version)],
                      static_cast<unsigned>(machine.configuration.kind),
                      machine.cd_session->disc.toc().tracks.size(),
                      machine.cd_session->disc.identity().c_str());
        return 0;
    }
    machine.cd_controller.connect(nullptr, {});
    rxpce_rom::Cartridge image;
    if (!rxpce_rom::take(image) || image.source_path.empty() || image.bytes.size() < 8192 ||
        image.bytes.size() % 8192)
        return -1;
    machine.set_rom(std::move(image.bytes), image.profile.flags);
    machine.source = std::move(image.source_path);
    machine.bank_log_count = 0;
    LogDumpDetail("Cartridge profile: %s flags=%u prefix_sha256=%s\n", image.profile.name,
                  CartridgeFlags(), image.profile.identity.prefix_sha256.c_str());
    LogDumpDetail("Cartridge loaded: %s member=%s banks=%d\n",
                  rxpce_paths::utf8(machine.source).c_str(), image.member_name.c_str(),
                  static_cast<int>(machine.rom_banks()));
    return 0;
}
void ResetPCE(PceCpu* state) {
    auto& machine = CurrentMachine();
    RestoreCartridgeBank(0);
    machine.io = {};
    std::array<std::uint8_t, 5> modes{};
    for (unsigned n = 0; n < 5; ++n)
        modes[n] = static_cast<std::uint8_t>(rxpce_config::active_input.players[n].buttons);
    machine.io.controllers.configure(rxpce_config::active_input.count, modes);
    machine.input_read_log = 0;
    std::fill_n(machine.video.palette.data(), 512, 0);
    machine.video.render_frame = true;
    ResetFrameTiming();
    machine.scanline = 0;
    machine.io.vdc_status = 0;
    machine.io.vdc_inc = 1;
    machine.io.minline = 0;
    machine.io.maxline = 255;
    machine.io.irq_mask = 0;
    machine.io.psg_volume = 0;
    machine.io.psg_ch = 0;
    for (int i = 0; i < 6; i++) {
        machine.io.psg[i][4] = 0x80;
    }

    ResetPceCpu(*state, machine.bus(), machine.line_period, machine.timer_period);
    if (machine.cd_session) {
        machine.cd_controller.reset(rxpce_host::runtime.base_clock_hz);
        machine.cd_execution_logged = false;
    }
#ifdef SOUND
    ResetSound(machine.cpu.cycles(), rxpce_audio::snapshot_psg(machine.io));
#endif
}

static int InitPCE(const std::filesystem::path& backmemname) {
    auto& machine = CurrentMachine();
    machine.backup_writable = false;
    machine.cartridge_ram_writable = false;
    machine.ram_path.clear();
    ConnectMachine(machine);
    if (CartLoad())
        return -1;
    if (!machine.graphics.initialize()) {
        SetInfoString("Can't allocate video memory");
        return -1;
    }
    std::fill_n(machine.populous.data(), (machine.populous.size() * sizeof(machine.populous[0])),
                0);
    machine.ram_path.clear();
    machine.cartridge_ram_writable = false;
    if (CartridgeFlags() & rxpce_rom::populous_ram) {
        StateFilePath(); // Create the configured state directory as for F5.
        machine.ram_path = rxpce_rom::ram_path(machine.source, rxpce_host::runtime.state_directory);
        std::string error;
        machine.cartridge_ram_writable =
            rxpce_rom::load_ram(machine.ram_path, machine.populous.data(), error);
        if (!machine.cartridge_ram_writable) {
            LogDump("%s: %s\n", error.c_str(), rxpce_paths::utf8(machine.ram_path).c_str());
            SetInfoString("Can't load cartridge RAM file");
        } else
            LogDumpDetail("Cartridge RAM ready: %s\n", rxpce_paths::utf8(machine.ram_path).c_str());
    }
#define VRAMSIZE 0x20000

    std::fill_n(machine.ram.data(), (machine.ram.size() * sizeof(machine.ram[0])), 0);
    std::fill_n(machine.cd_ram.data(), (machine.cd_ram.size() * sizeof(machine.cd_ram[0])), 0);
    std::fill_n(machine.super_ram.data(), (machine.super_ram.size() * sizeof(machine.super_ram[0])),
                0);
    std::fill_n(machine.backup_ram.data(),
                (machine.backup_ram.size() * sizeof(machine.backup_ram[0])), 0);
    std::string error;
    machine.backup_writable = rxpce_storage::load_nvram(
        backmemname.native(), machine.backup_ram.data(),
        (machine.backup_ram.size() * sizeof(machine.backup_ram[0])), error);
    if (!machine.backup_writable)
        LogDump("Backup RAM load failed: %s (%s)\n", error.c_str(),
                rxpce_paths::utf8(backmemname).c_str());
    else
        LogDumpDetail("Backup RAM ready: %s bytes=%zu\n", rxpce_paths::utf8(backmemname).c_str(),
                      (machine.backup_ram.size() * sizeof(machine.backup_ram[0])));
    return 0;
}

static int RunPCE(void) {
    auto& machine = CurrentMachine();
    ResetPCE(&machine.cpu);
    if (!machine.backup_writable)
        SetInfoString("Can't load backup.dat; existing file will be preserved");
    if (!machine.ram_path.empty() && !machine.cartridge_ram_writable)
        SetInfoString("Can't load cartridge RAM file");
    PceCpuHost host{
        machine.bus(),
        [](void*) { return IsMachineStopRequested(); },
        [](PceCpu&, void* context) {
            return rxpce_machine::TickMachineLine(*static_cast<Machine*>(context), NativeDevices(),
                                                  HostFrame());
        },
        [](PceCpu&, void* context) {
            rxpce_machine::TickMachineTimer(*static_cast<Machine*>(context));
        },
        [](void* context) -> byte { return static_cast<Machine*>(context)->io.irq_status & 7; },
        [](void* context) -> byte { return static_cast<Machine*>(context)->io.irq_mask & 7; },
#ifdef _DEBUG
        [](PceCpu& cpu, void* context) {
            return DebugTrace(&cpu, static_cast<Machine*>(context)->bus());
        },
#else
        nullptr,
#endif
        &machine};
    host.clock = [](PceCpu& cpu, void* context) {
        auto& machine = *static_cast<Machine*>(context);
        if (machine.cd_session) {
            machine.cd_controller.advance(cpu.cpu.elapsed);
            machine.io.irq_status =
                (machine.io.irq_status & ~IRQ2) | (machine.cd_controller.irq() ? IRQ2 : 0);
        }
    };
    LogDumpDetail("CPU core=MCS6502+HuC6280\n");
    if (RunPceCpu(machine.cpu, host) == PceCpuResult::invalid_instruction) {
        LogDumpDetail("CPU stopped: unsupported instruction at PC=%04x\n", machine.cpu.cpu.pc);
        SetInfoString("Unsupported CPU instruction");
    }
    return 1;
}

static void TrashPCE(const std::filesystem::path& backmemname) {
    auto& machine = CurrentMachine();
    if (machine.cartridge_ram_writable && !machine.ram_path.empty()) {
        std::string error;
        if (!rxpce_rom::save_ram(machine.ram_path, machine.populous.data(), error)) {
            LogDump("%s: %s\n", error.c_str(), rxpce_paths::utf8(machine.ram_path).c_str());
            SetInfoString("Can't save cartridge RAM file");
        } else
            LogDumpDetail("Cartridge RAM saved: %s\n", rxpce_paths::utf8(machine.ram_path).c_str());
    }
    if (machine.backup_writable) {
        std::string error;
        if (!rxpce_storage::save_nvram(backmemname.native(), machine.backup_ram.data(),
                                       (machine.backup_ram.size() * sizeof(machine.backup_ram[0])),
                                       error)) {
            LogDump("Backup RAM save failed: %s (%s)\n", error.c_str(),
                    rxpce_paths::utf8(backmemname).c_str());
            SetInfoString("Can't save backup.dat");
        } else
            LogDumpDetail("Backup RAM saved: %s bytes=%zu\n",
                          rxpce_paths::utf8(backmemname).c_str(),
                          (machine.backup_ram.size() * sizeof(machine.backup_ram[0])));
    }
    machine.graphics.reset();

#ifdef SOUND
    SetSoundExtraSource({});
#endif
    machine.clear_media();
    cd_running = false;
    machine.configuration.kind = rxpce_machine::Kind::hucard;
}

int rxpce_host::RunEmulation(const std::filesystem::path& backup_file) {
    int result = -1;
    bool core_started = false;
    try {
        auto& machine = CurrentMachine();
        machine.line_period = rxpce_host::runtime.base_clock_hz / (machine.frame_lines * 60);
        LogDumpDetail("Emulation clock=%d period=%d\n", rxpce_host::runtime.base_clock_hz,
                      machine.line_period);
        machine.timer_period = 1024;
        if (rxpce_host::InitializeHost(machine)) {
            core_started = true;
            if (InitPCE(backup_file) == 0) {
                SetInfoString("Emulation started");
                RunPCE();
                result = 0;
            }
        }
    } catch (const std::exception& error) {
        LogDump("Emulation failed: %s\n", error.what());
        SetInfoString("Emulation failed");
    }
    if (core_started)
        TrashPCE(backup_file);
    rxpce_host::ShutdownHost();
    return result;
}
