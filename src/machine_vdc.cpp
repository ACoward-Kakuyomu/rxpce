#include "machine_vdc.h"
#include "machine.h"
#include "vdc_registers.h"
#include <algorithm>
#include <stdexcept>

namespace rxpce_machine {
namespace {
using namespace rxpce_video;
using namespace vdc;
std::uint16_t read_word(Machine& m, std::uint16_t address) {
    const auto* p = m.graphics.vram() + unsigned(address) * 2;
    return static_cast<std::uint16_t>(p[0] | (unsigned(p[1]) << 8));
}
void write_word(Machine& m, std::uint16_t address, std::uint16_t value) {
    auto* p = m.graphics.vram() + unsigned(address) * 2;
    p[0] = static_cast<std::uint8_t>(value);
    p[1] = static_cast<std::uint8_t>(value >> 8);
    m.graphics.background_dirty()[address / 16] = 1;
    m.graphics.sprite_dirty()[address / 64] = 1;
}
DmaCallbacks callbacks(Machine& m) {
    return {&m,
            [](void* p, std::uint16_t a, std::uint64_t) {
                return read_word(*static_cast<Machine*>(p), a);
            },
            [](void* p, std::uint16_t a, std::uint16_t v, std::uint64_t) {
                write_word(*static_cast<Machine*>(p), a, v);
            },
            [](void* p, unsigned i, std::uint16_t v, std::uint64_t) {
                static_cast<Machine*>(p)->video.sprites[i] =
                    static_cast<std::int16_t>(v <= 0x7fff ? int(v) : int(v) - 65536);
            },
            [](void* p, std::uint16_t s, std::uint16_t d, std::uint16_t n) {
                auto& x = *static_cast<Machine*>(p);
                x.io.vdc[SOUR].value = s;
                x.io.vdc[DISTR].value = d;
                x.io.vdc[LENR].value = n;
            },
            [](void* p, DmaChannel c, std::uint64_t) {
                auto& x = *static_cast<Machine*>(p);
                const unsigned enable = c == DmaChannel::satb ? 1 : 2;
                if (x.io.vdc[DCR].value & enable) {
                    x.io.vdc_status |= c == DmaChannel::satb ? VDC_SATBfinish : VDC_DMAfinish;
                    x.io.irq_status |= 2;
                    x.vdc.last_dma_irq_time = x.vdc_mode == VdcMode::fast
                                                  ? x.cpu.cpu.elapsed
                                                  : x.vdc.clock.state().cpu_time;
                }
            }};
}
bool cpu_slot(const Machine& m, const VdcTimingState& t, bool open, std::uint64_t dot) {
    if (open)
        return true;
    const auto start =
        std::uint64_t(((m.io.vdc[HSR].value & 31) + 1) + (((m.io.vdc[HSR].value >> 8) & 127) + 1)) *
        8;
    const auto end = start + std::uint64_t((m.io.vdc[HDR].value & 127) + 1) * 8;
    if (dot >= start && dot < end && (t.line_cr & 0x80)) {
        const unsigned phase = static_cast<unsigned>((dot - start) & 7);
        switch (t.active_mwr & 3) {
        case 0:
            return !(phase & 1);
        case 1:
        case 2:
            return phase == 2 || phase == 3;
        default:
            return false;
        }
    }
    if ((t.line_cr & 0x40) && dot >= end) {
        unsigned cells = 0;
        for (unsigned i = 0; i < 64 && cells < 16; ++i) {
            const auto* s = m.video.sprites.data() + i * 4;
            const int y = (s[0] & 1023) - 64;
            constexpr int heights[] = {16, 32, 64, 64};
            if (int(t.line) >= y && int(t.line) < y + heights[(s[3] >> 12) & 3])
                cells += ((s[3] & 0x100) ? 2 : 1);
        }
        constexpr unsigned costs[] = {4, 4, 8, 16};
        if (dot - end < std::min(cells, 16u) * costs[(t.active_mwr >> 2) & 3])
            return false;
    }
    return true;
}
void service_cpu(Machine& m, const VdcTimingState& t, bool window, std::uint64_t dot) {
    auto& q = m.vdc.cpu;
    if (q.kind == CpuVramKind::none || !cpu_slot(m, t, window, dot))
        return;
    constexpr unsigned widths[] = {1, 2, 2, 4};
    if (++q.elapsed < widths[t.active_mwr & 3])
        return;
    if (q.kind == CpuVramKind::read)
        q.read_buffer = read_word(m, q.address);
    else {
        write_word(m, q.address, q.value);
        m.io.vdc[MAWR].value = static_cast<std::uint16_t>(q.address + q.increment);
    }
    q.kind = CpuVramKind::none;
    q.elapsed = 0;
}
} // namespace
void ResetMachineVdc(Machine& m) {
    if (m.line_period < 1 || m.frame_lines < 2)
        throw std::invalid_argument("Invalid machine VDC clock");
    const auto last =
        std::min<unsigned>(static_cast<unsigned>(std::max(0, m.io.maxline)), m.frame_lines - 2);
    const auto first = std::min<unsigned>(static_cast<unsigned>(std::max(0, m.io.minline)), last);
    m.vdc = VdcDevice{};
    m.vdc.clock = VdcTiming(
        {static_cast<unsigned>(m.line_period), static_cast<unsigned>(m.frame_lines), first, last},
        {0, m.io.vdc[CR].value, m.io.vdc[MWR].value});
}
void AdvanceMachineVdc(Machine& m, std::uint64_t target) {
    if (m.vdc_mode == VdcMode::fast)
        return;
    auto& device = m.vdc;
    if (device.clock.state().cpu_time == 0 && device.clock.state().dots == 0 &&
        !device.dma.busy() && device.cpu.kind == CpuVramKind::none &&
        (device.clock.config().line_period != unsigned(m.line_period) ||
         device.clock.config().frame_lines != unsigned(m.frame_lines)))
        ResetMachineVdc(m);
    if (target < device.clock.state().cpu_time)
        throw std::invalid_argument("Machine VDC time reversal");
    const auto last = std::min<unsigned>(static_cast<unsigned>(std::max(0, m.io.maxline)),
                                         device.clock.config().frame_lines - 2);
    device.clock.set_display_range(
        std::min<unsigned>(static_cast<unsigned>(std::max(0, m.io.minline)), last), last);
    const auto cb = callbacks(m);
    while (device.clock.state().cpu_time < target) {
        const auto before = device.clock.state();
        const bool window = device.clock.dma_window_open();
        const auto next = (device.dma.busy() || device.cpu.kind != CpuVramKind::none)
                              ? before.cpu_time + 1
                              : std::min(target, device.clock.next_line_time());
        device.clock.advance_to(std::min(target, next));
        const auto& after = device.clock.state();
        const bool boundary = before.line != after.line;
        const bool display_start = boundary && after.line == device.clock.config().first_active;
        const auto exclude =
            display_start && !after.burst && after.dot_remainder == 0 && after.dots > before.dots
                ? 1u
                : 0u;
        if (device.dma.busy() || device.cpu.kind != CpuVramKind::none) {
            for (auto dot = before.dots + 1; dot <= after.dots - exclude; ++dot) {
                const bool occupied = window && device.dma.busy();
                device.dma.advance_to(dot, window, cb);
                if (!occupied)
                    service_cpu(m, before, window, before.line_dots + (dot - before.dots));
            }
            if (exclude)
                device.dma.advance_to(after.dots, false, cb);
        } else
            device.dma.advance_to(after.dots, window, cb);
        if (boundary) {
            if (display_start)
                device.dma.display_start(after.burst);
            if (after.line == device.clock.config().last_active + 1)
                device.dma.vblank_start((m.io.vdc[DCR].value & 16) != 0, m.io.vdc[SATB].value);
        }
    }
}
std::uint16_t ReadVramDirect(Machine& m, std::uint16_t address) {
    return read_word(m, address);
}
void WriteVramDirect(Machine& m, std::uint16_t address, std::uint16_t value) {
    write_word(m, address, value);
}
void TransferFastVram(Machine& m) {
    // Reuse the word engine's overlap, wrapping, registers and dirty rules.
    // Its local clock is a work budget, not a running machine device clock.
    VdcDma job;
    job.request_vram(m.io.vdc[SOUR].value, m.io.vdc[DISTR].value, m.io.vdc[LENR].value,
                     m.io.vdc[DCR].value);
    job.advance_to((std::uint64_t(m.io.vdc[LENR].value) + 1) * 4, true, callbacks(m));
}
void TickFastVdc(Machine& m) {
    auto state = m.vdc.dma.state();
    if (m.scanline == m.io.maxline + 1 && (state.satb_requested || (m.io.vdc[DCR].value & 16))) {
        VdcDma job;
        job.request_satb(state.satb_requested ? state.satb_request_source : m.io.vdc[SATB].value);
        job.vblank_start(false, 0);
        auto cb = callbacks(m);
        cb.complete = [](void*, DmaChannel, std::uint64_t) {};
        job.advance_to(1024, true, cb);
        state.satb_requested = false;
        state.satb_running = true; // Fast: data copied, notification still pending.
        m.vdc.dma.restore(state);
    } else if (m.scanline == m.io.maxline + 5 && state.satb_running) {
        state.satb_running = false;
        m.vdc.dma.restore(state);
        const auto cb = callbacks(m);
        cb.complete(cb.context, DmaChannel::satb, 0);
    }
}
void QueueVramRead(Machine& m, std::uint16_t address) {
    if (m.vdc.cpu.kind != rxpce_video::CpuVramKind::none)
        throw std::logic_error("VRAM read queue is full");
    m.vdc.cpu.kind = rxpce_video::CpuVramKind::read;
    m.vdc.cpu.address = address;
    m.vdc.cpu.elapsed = 0;
}
void QueueVramWrite(Machine& m, std::uint16_t address, std::uint16_t value) {
    if (m.vdc.cpu.kind != rxpce_video::CpuVramKind::none)
        throw std::logic_error("VRAM write queue is full");
    m.vdc.cpu.kind = rxpce_video::CpuVramKind::write;
    m.vdc.cpu.address = address;
    m.vdc.cpu.value = value;
    m.vdc.cpu.increment = m.io.vdc_inc;
    m.vdc.cpu.elapsed = 0;
}
unsigned MachineVdcReady(Machine& m, std::uint32_t address, bool write) {
    if ((address >> 13) != 255 || (address & 0x1c00) != 0 ||
        m.vdc.cpu.kind == rxpce_video::CpuVramKind::none)
        return 0;
    const auto port = address & 3;
    return ((!write && (port == 2 || port == 3) && m.io.vdc_reg == 2) ||
            (write && port == 3 && (m.io.vdc_reg == 2 || m.io.vdc_reg == 1)))
               ? 1
               : 0;
}
} // namespace rxpce_machine
