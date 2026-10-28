#include "pce_cpu.h"
#include <cassert>
void ResetPceCpu(PceCpu& state, rxpce_cpu::Bus bus, int line, int timer) {
    assert(line > 0 && timer > 0);
    const auto revision = state.revision + 1;
    state = PceCpu{};
    state.revision = revision;
    state.line_period = state.line_remaining = line;
    state.timer_period = state.timer_remaining = timer;
    rxpce_cpu::HuC6280 cpu(bus, state.cpu);
    cpu.reset({{0xff, 0xf8, 1, 2, 3, 4, 5, 0}});
}
PceCpuResult RunPceCpu(PceCpu& state, const PceCpuHost& host) {
    rxpce_cpu::HuC6280 cpu(host.bus, state.cpu);
    for (;;) {
        if (host.stop(host.context))
            return PceCpuResult::stopped;
        if (host.trace && (state.Trace || state.cpu.pc == state.Trap)) {
            state.Trace = true;
            if (!host.trace(state, host.context))
                return PceCpuResult::stopped;
        }
        if (host.clock)
            host.clock(state, host.context);
        cpu.set_irq_mask(host.irq_mask(host.context));
        cpu.set_irq_lines(host.irq_lines(host.context));
        const auto result = cpu.step();
        if (!result.valid)
            return PceCpuResult::invalid_instruction;
        const int elapsed = static_cast<int>(result.elapsed);
        state.line_remaining -= elapsed;
        state.timer_remaining -= elapsed;
        while (state.timer_remaining <= 0) {
            if (host.stop(host.context))
                return PceCpuResult::stopped;
            state.timer_remaining += state.timer_period;
            host.timer(state, host.context);
        }
        while (state.line_remaining <= 0) {
            if (host.stop(host.context))
                return PceCpuResult::stopped;
            // Advance before callbacks: save/load and reset may replace all state.
            state.line_remaining += state.line_period;
            if (!host.scanline(state, host.context))
                return PceCpuResult::stopped;
        }
    }
}
