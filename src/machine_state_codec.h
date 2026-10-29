#pragma once
#include "io_state_codec.h"
#include "pce_cpu_state.h"
#include "video_frame.h"
#include "audio_state.h"
namespace rxpce_machine {
class Machine;
struct FrameSurface;
} // namespace rxpce_machine
#include <array>
#include <memory>

namespace rxpce_state {
namespace machine_wire {
constexpr std::size_t ram = 0, vram = 8192, sprites = 139264, palette = 139776, scanline = 140800;
constexpr std::size_t io = 140804, backup = 142206, bank = 144254, cpu = 144255,
                      base_size = cpu + cpu_state_size;
} // namespace machine_wire
struct MachineSnapshot {
    std::array<std::uint8_t, 8192> ram{};
    std::array<std::uint8_t, 0x20000> vram{};
    std::array<std::int16_t, 256> sprites{};
    std::array<rxpce_video::Pixel, 512> palette{};
    std::int32_t scanline = 0;
    rxpce_machine::IoState io;
    std::array<std::uint8_t, 2048> backup{};
    std::uint8_t bank = 0;
    PceCpu cpu;
    rxpce_audio::PsgPhaseState sound;
    std::array<std::uint8_t, 32768> populous{};
};
struct SnapshotRequirements {
    std::uint32_t flags = 0;
    int line_period = 1, timer_period = 1;
    unsigned frame_lines = 263, controllers = 1;
    std::array<std::uint8_t, 5> buttons{{2, 2, 2, 2, 2}};
};
constexpr std::size_t machine_state_bytes(std::uint32_t flags) {
    return machine_wire::base_size + ((flags & 1) ? 32768 : 0);
}
constexpr std::size_t hucard_state_bytes(std::uint32_t flags) {
    return machine_state_bytes(flags) + rxpce_audio::phase_state_bytes;
}
// Capture base machine state; the host attaches its synchronized PSG phase.
std::unique_ptr<MachineSnapshot> capture_machine(const rxpce_machine::Machine&,
                                                 std::uint32_t flags);
// Copy validated RAM/I/O/CPU and invalidate derived video.
// Mapper, CD and host audio restoration remain with the owning service.
void apply_machine(rxpce_machine::Machine&, const MachineSnapshot&, std::uint32_t flags,
                   rxpce_machine::FrameSurface);
std::vector<char> encode_hucard(const MachineSnapshot&, const SnapshotRequirements&);
bool decode_hucard(const char*, std::size_t, const SnapshotRequirements&,
                   std::unique_ptr<MachineSnapshot>&, std::string& error);
bool valid_machine(const MachineSnapshot&, const SnapshotRequirements&, std::string& error);
std::vector<char> encode_machine(const MachineSnapshot&, const SnapshotRequirements&);
bool decode_machine(const char* data, std::size_t size, const SnapshotRequirements&,
                    std::unique_ptr<MachineSnapshot>& output, std::string& error);
} // namespace rxpce_state
