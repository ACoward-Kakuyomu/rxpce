#include "machine_state_codec.h"
#include "machine.h"
#include "machine_devices.h"
#include <algorithm>
namespace rxpce_state {
std::unique_ptr<rxpce_state::MachineSnapshot> capture_machine(const rxpce_machine::Machine& machine,
                                                              std::uint32_t flags) {
    auto value = std::make_unique<rxpce_state::MachineSnapshot>();
    std::copy_n(machine.ram.data(), value->ram.size(), value->ram.begin());
    std::copy_n(machine.graphics.vram(), value->vram.size(), value->vram.begin());
    std::copy_n(machine.video.sprites.data(), value->sprites.size(), value->sprites.begin());
    std::copy_n(machine.video.palette.data(), value->palette.size(), value->palette.begin());
    value->scanline = machine.scanline;
    value->io = machine.io;
    std::copy_n(machine.backup_ram.data(), value->backup.size(), value->backup.begin());
    value->bank = machine.mapper().selection();
    value->cpu = machine.cpu;
    value->vdc = {machine.vdc.scan.state(),
                  machine.vdc.dma.state(),
                  machine.vdc.cpu,
                  machine.vdc.sprite_fetch_cells,
                  machine.vdc.sprite_fetch_remaining,
                  machine.vdc.fast_satb_lines};
    value->video_clock = {machine.cpu_clock_hz, machine.video_clock.state(),
                          machine.vce_clock.state()};
    if (flags & 1)
        std::copy_n(machine.populous.data(), value->populous.size(), value->populous.begin());
    return value;
}
void apply_machine(rxpce_machine::Machine& machine, const MachineSnapshot& value,
                   std::uint32_t flags, rxpce_machine::FrameSurface frame) {
    std::copy(value.ram.begin(), value.ram.end(), machine.ram.data());
    std::copy(value.vram.begin(), value.vram.end(), machine.graphics.vram());
    std::copy(value.sprites.begin(), value.sprites.end(), machine.video.sprites.data());
    std::copy(value.palette.begin(), value.palette.end(), machine.video.palette.data());
    machine.cpu_clock_hz = value.video_clock.cpu_hz;
    machine.video_clock = rxpce_video::CpuVideoClock(machine.cpu_clock_hz);
    machine.video_clock.restore(value.video_clock.cpu);
    machine.vce_clock.restore(value.video_clock.vce);
    machine.frame_lines = value.video_clock.vce.frame_lines;
    machine.scanline = value.scanline;
    machine.io = value.io;
    std::copy(value.backup.begin(), value.backup.end(), machine.backup_ram.data());
    const auto revision = machine.cpu.revision + 1;
    machine.cpu = value.cpu;
    machine.line_period = value.cpu.line_period;
    machine.timer_period = value.cpu.timer_period;
    machine.cpu.revision = revision;
    machine.vdc.scan.restore(value.vdc.scan);
    machine.vdc.dma.restore(value.vdc.dma);
    machine.vdc.cpu = value.vdc.cpu;
    machine.vdc.sprite_fetch_cells = value.vdc.sprite_fetch_cells;
    machine.vdc.sprite_fetch_remaining = value.vdc.sprite_fetch_remaining;
    machine.vdc.fast_satb_lines = value.vdc.fast_satb_lines;
    machine.vdc.last_dma_irq_time = 0;
    if (flags & 1)
        std::copy(value.populous.begin(), value.populous.end(), machine.populous.data());
    rxpce_machine::ResetVideoAfterLoad(machine, frame);
}
} // namespace rxpce_state
