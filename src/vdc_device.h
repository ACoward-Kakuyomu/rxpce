#pragma once
#include "vdc_timing.h"
#include "vdc_dma.h"
namespace rxpce_video {
struct VdcDevice {
    VdcTiming clock{VdcTimingConfig{1, 263, 0, 255}};
    VdcDma dma;
    CpuVramAccess cpu;
    // Diagnostic observation only; not part of resumable device state.
    std::uint64_t last_dma_irq_time = 0;
};
} // namespace rxpce_video
