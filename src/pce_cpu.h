#pragma once
#include "huc6280.h"
#include "pce_types.h"
struct PceCpu {
    rxpce_cpu::State cpu;
    int line_period = 1, line_remaining = 1, timer_period = 1, timer_remaining = 1;
    bool Trace = false;
    word Trap = 0xffff;
    std::uint64_t revision = 0; // Runtime-only invalidation for callbacks; not serialized.
    std::uint32_t cycles() const {
        return static_cast<std::uint32_t>(cpu.elapsed);
    }
};
struct PceCpuHost {
    rxpce_cpu::Bus bus;
    bool (*stop)(void*);
    bool (*scanline)(PceCpu&, void*);
    void (*timer)(PceCpu&, void*);
    byte (*irq_lines)(void*);
    byte (*irq_mask)(void*);
    bool (*trace)(PceCpu&, void*);
    void* context;
    void (*clock)(PceCpu&, void*) = nullptr;
};
enum class PceCpuResult { stopped, invalid_instruction };
void ResetPceCpu(PceCpu&, rxpce_cpu::Bus, int line_period, int timer_period);
PceCpuResult RunPceCpu(PceCpu&, const PceCpuHost&);
