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
    value->vdc = {machine.vdc.clock.config(), machine.vdc.clock.state(), machine.vdc.dma.state(),
                  machine.vdc.cpu};
    if (machine.vdc_mode == rxpce_video::VdcMode::fast) {
        value->vdc.config.first_active = static_cast<unsigned>(machine.io.minline);
        value->vdc.config.last_active = static_cast<unsigned>(machine.io.maxline);
    }
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
    machine.scanline = value.scanline;
    machine.io = value.io;
    std::copy(value.backup.begin(), value.backup.end(), machine.backup_ram.data());
    const auto revision = machine.cpu.revision + 1;
    machine.cpu = value.cpu;
    machine.cpu.revision = revision;
    machine.vdc.clock = rxpce_video::VdcTiming(value.vdc.config);
    machine.vdc.clock.restore(value.vdc.timing);
    machine.vdc.dma.restore(value.vdc.dma);
    machine.vdc.cpu = value.vdc.cpu;
    machine.vdc.last_dma_irq_time = 0;
    if (flags & 1)
        std::copy(value.populous.begin(), value.populous.end(), machine.populous.data());
    rxpce_machine::ResetVideoAfterLoad(machine, frame);
}
} // namespace rxpce_state
