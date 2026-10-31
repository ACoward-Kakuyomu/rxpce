#include "pce_cpu.h"
#include <cassert>
#include <algorithm>
#include <limits>
#include <stdexcept>
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
    const auto start = state.cpu.elapsed - elapsed;
    if (state.device_time < start)
        state.device_time = start;
    SynchronizePceCpuDevices(state, state.cpu.elapsed, host);
}
void SynchronizePceCpuDevices(PceCpu& state, std::uint64_t target, const PceCpuHost& host) {
    assert(state.line_period > 0 && state.timer_period > 0 && state.line_remaining > 0 &&
           state.timer_remaining > 0 && state.device_time <= target && target <= state.cpu.elapsed);
    auto next_line = [&] {
        if (!host.line_deadline)
            return state.line_period;
        const auto deadline = host.line_deadline(state, host.context);
        if (deadline <= state.device_time ||
            deadline - state.device_time > static_cast<unsigned>(state.line_period))
            throw std::invalid_argument("Invalid absolute scanline deadline");
        return static_cast<int>(deadline - state.device_time);
    };
    if (host.line_deadline)
        state.line_remaining = next_line();
    while (state.device_time < target || state.line_remaining == 0 || state.timer_remaining == 0) {
        const auto delta = std::min<std::uint64_t>(
            target - state.device_time,
            static_cast<unsigned>(std::min(state.line_remaining, state.timer_remaining)));
        state.line_remaining -= static_cast<int>(delta);
        state.timer_remaining -= static_cast<int>(delta);
        state.device_time += delta;
        if (host.clock)
            host.clock(state, host.context);
        if (state.timer_remaining == 0) {
            state.timer_remaining = state.timer_period;
            host.timer(state, host.context);
        } else if (state.line_remaining == 0) {
            // Recompute only after the scanline consumes the old boundary.
            state.line_remaining = state.line_period;
            host.scanline(state, host.context);
            state.line_remaining = next_line();
        }
    }
}
PceCpuResult RunPceCpu(PceCpu& state, const PceCpuHost& host) {
    struct Access {
        PceCpu& cpu;
        const PceCpuHost& host;
    } access{state, host};
    auto bus = host.bus;
    if (bus.synchronize || bus.ready) {
        bus = {[](std::uint32_t a, void* p) {
                   auto& x = *static_cast<Access*>(p);
                   return x.host.bus.read(a, x.host.bus.context);
               },
               [](std::uint32_t a, std::uint8_t v, void* p) {
                   auto& x = *static_cast<Access*>(p);
                   x.host.bus.write(a, v, x.host.bus.context);
               },
               &access,
               [](std::uint32_t a, bool w, void* p) -> unsigned {
                   auto& x = *static_cast<Access*>(p);
                   return x.host.bus.wait ? x.host.bus.wait(a, w, x.host.bus.context) : 0;
               },
               [](std::uint64_t t, void* p) {
                   auto& x = *static_cast<Access*>(p);
                   if (t > x.cpu.device_time)
                       SynchronizePceCpuDevices(x.cpu, t, x.host);
                   if (x.host.bus.synchronize)
                       x.host.bus.synchronize(t, x.host.bus.context);
               },
               [](std::uint32_t a, bool w, void* p) -> unsigned {
                   auto& x = *static_cast<Access*>(p);
                   return x.host.bus.ready ? x.host.bus.ready(a, w, x.host.bus.context) : 0;
               }};
    }
    rxpce_cpu::HuC6280 cpu(bus, state.cpu);
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
