#pragma once
#include "vdc_scan.h"
#include "vdc_dma.h"
namespace rxpce_video {
struct VdcDevice {
    VdcScan scan{};
    VdcDma dma;
    CpuVramAccess cpu;
    std::uint16_t sprite_fetch_remaining = 0;
    std::uint8_t fast_satb_lines = 0;
    std::uint8_t sprite_fetch_cells = 0; // Latched at each line; resumable CPU bus reservation.
    // Diagnostic observation only; not part of resumable device state.
    std::uint64_t last_dma_irq_time = 0;
};
} // namespace rxpce_video
