#pragma once
#include "huc6280.h"
#include "pce_types.h"
struct PceCpu {
    rxpce_cpu::State cpu;
    int line_period = 1, line_remaining = 1, timer_period = 1, timer_remaining = 1;
    bool Trace = false;
    word Trap = 0xffff;
    std::uint64_t revision = 0;    // Runtime-only invalidation for callbacks; not serialized.
    std::uint64_t device_time = 0; // Runtime-only timestamp of the current device callback.
    std::uint32_t cycles() const {
        return static_cast<std::uint32_t>(cpu.elapsed);
    }
};
struct PceCpuHost {
    rxpce_cpu::Bus bus;
    bool (*stop)(void*);
    void (*scanline)(PceCpu&, void*);
    void (*timer)(PceCpu&, void*);
    byte (*irq_lines)(void*);
    byte (*irq_mask)(void*);
    bool (*trace)(PceCpu&, void*);
    void* context;
    void (*clock)(PceCpu&, void*) = nullptr;
    bool (*safe_point)(PceCpu&, void*) = nullptr;
    // Optional absolute next line deadline. Must be strictly after device_time,
    // at most line_period ticks away; callbacks may not replace CPU state.
    std::uint64_t (*line_deadline)(const PceCpu&, void*) = nullptr;
};
enum class PceCpuResult { stopped, invalid_instruction };
void ResetPceCpu(PceCpu&, rxpce_cpu::Bus, int line_period, int timer_period);
// cpu.elapsed is already the end time; callbacks must not replace CPU state.
// Host reset/load operations belong to safe_point after all debt is drained.
void AdvancePceCpuDevices(PceCpu&, unsigned elapsed, const PceCpuHost&);
// Synchronize an elapsed prefix inside a bus wait. CPU elapsed already includes that prefix.
void SynchronizePceCpuDevices(PceCpu&, std::uint64_t target, const PceCpuHost&);
PceCpuResult RunPceCpu(PceCpu&, const PceCpuHost&);
