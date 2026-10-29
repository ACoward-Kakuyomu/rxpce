#include "pce_cpu.h"
#include <cassert>
#include <limits>
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
void AdvancePceCpuDevices(PceCpu& state, unsigned elapsed, const PceCpuHost& host) {
    assert(state.line_period > 0 && state.timer_period > 0 && state.line_remaining > 0 &&
           state.timer_remaining > 0 && elapsed <= unsigned(std::numeric_limits<int>::max()) &&
           state.cpu.elapsed >= elapsed);
    state.line_remaining -= static_cast<int>(elapsed);
    state.timer_remaining -= static_cast<int>(elapsed);
    auto sync = [&](std::uint64_t time) {
        state.device_time = time;
        if (host.clock)
            host.clock(state, host.context);
    };
    while (state.timer_remaining <= 0 || state.line_remaining <= 0) {
        const bool timer = state.timer_remaining <= state.line_remaining;
        const auto debt = timer ? state.timer_remaining : state.line_remaining;
        sync(state.cpu.elapsed - static_cast<std::uint64_t>(-std::int64_t(debt)));
        if (timer) {
            state.timer_remaining += state.timer_period;
            host.timer(state, host.context);
        } else {
            state.line_remaining += state.line_period;
            host.scanline(state, host.context);
        }
    }
    sync(state.cpu.elapsed);
}
PceCpuResult RunPceCpu(PceCpu& state, const PceCpuHost& host) {
    rxpce_cpu::HuC6280 cpu(host.bus, state.cpu);
    bool clock_ready = false;
    for (;;) {
        if (host.stop(host.context))
            return PceCpuResult::stopped;
        if (!state.cpu.transfer.opcode && host.trace &&
            (state.Trace || state.cpu.pc == state.Trap)) {
            state.Trace = true;
            if (!host.trace(state, host.context))
                return PceCpuResult::stopped;
        }
        if (!clock_ready) {
            state.device_time = state.cpu.elapsed;
            if (host.clock)
                host.clock(state, host.context);
        }
        cpu.set_irq_mask(host.irq_mask(host.context));
        cpu.set_irq_lines(host.irq_lines(host.context));
        const auto result = cpu.step();
        if (!result.valid)
            return PceCpuResult::invalid_instruction;
        AdvancePceCpuDevices(state, result.elapsed, host);
        const auto end = state.cpu.elapsed, revision = state.revision;
        if (host.safe_point && !host.safe_point(state, host.context))
            return PceCpuResult::stopped;
        clock_ready = state.cpu.elapsed == end && state.revision == revision;
    }
}
