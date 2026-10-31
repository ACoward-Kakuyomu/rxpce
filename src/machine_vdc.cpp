#include "machine_vdc.h"
#include "machine.h"
#include "vdc_render.h"
#include "vdc_registers.h"
#include <algorithm>
#include <stdexcept>
#include <limits>

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
                    x.vdc.last_dma_irq_time =
                        x.vdc_mode == VdcMode::fast
                            ? x.cpu.cpu.elapsed
                            : x.video_clock.cpu_deadline(x.vce_clock.state().master_time);
                }
            }};
}
bool cpu_slot(const Machine& m) {
    const auto& s = m.vdc.scan.state();
    if (m.vdc.scan.dma_window_open())
        return true;
    if (m.vdc.sprite_fetch_remaining)
        return false;
    if (s.h_phase == HPeriod::display && (s.active.control & 0x80)) {
        const auto phase = (((s.active.hdr & 127) + 1) * 8 - s.h_remaining) & 7;
        switch (s.active.memory & 3) {
        case 0:
            return !(phase & 1);
        case 1:
        case 2:
            return phase == 2 || phase == 3;
        default:
            return false;
        }
    }
    return true;
}
void service_cpu(Machine& m) {
    auto& q = m.vdc.cpu;
    if (q.kind == CpuVramKind::none || !cpu_slot(m))
        return;
    constexpr unsigned widths[] = {1, 2, 2, 4};
    if (++q.elapsed < widths[m.vdc.scan.state().active.memory & 3])
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
void fast_satb(Machine& m) {
    auto state = m.vdc.dma.state();
    if (!state.satb_requested && !(m.io.vdc[DCR].value & 16))
        return;
    VdcDma job;
    job.request_satb(state.satb_requested ? state.satb_request_source : m.io.vdc[SATB].value);
    job.vblank_start(false, 0);
    auto cb = callbacks(m);
    cb.complete = [](void*, DmaChannel, std::uint64_t) {};
    job.advance_to(1024, true, cb);
    state.satb_requested = false;
    m.vdc.dma.restore(state);
    m.vdc.fast_satb_lines = 4;
}
void scan_event(void* context, const ScanEvent& e) {
    auto& m = *static_cast<Machine*>(context);
    const auto& s = m.vdc.scan.state();
    auto raise = [&](unsigned status) {
        m.io.vdc_status |= static_cast<byte>(status);
        m.io.irq_status |= IRQ1;
    };
    switch (e.kind) {
    case ScanEventKind::hsync_start:
        if (m.vdc_mode == VdcMode::fast && m.vdc.fast_satb_lines && --m.vdc.fast_satb_lines == 0)
            callbacks(m).complete(&m, DmaChannel::satb, 0);
        break;
    case ScanEventKind::display_start:
        m.io.screen_h = s.active.vdw + 1;
        m.io.minline = 0;
        m.io.maxline = m.io.screen_h - 1;
        m.video.row_bias =
            (std::max)(0, ((std::min)(m.io.screen_h, m.frame_lines) - int(frame_height)) / 2);
        m.video.render_frame = m.video.next_render;
        if (m.vdc_mode == VdcMode::accurate)
            m.vdc.dma.display_start(s.burst);
        break;
    case ScanEventKind::vblank_start:
        m.io.bg_w = 32 << ((s.active.memory >> 4) & 3);
        if (m.io.bg_w > 128)
            m.io.bg_w = 128;
        m.io.bg_h = (s.active.memory & 64) ? 64 : 32;
        if (m.vdc_mode == VdcMode::fast)
            fast_satb(m);
        else
            m.vdc.dma.vblank_start((m.io.vdc[DCR].value & 16) != 0, m.io.vdc[SATB].value);
        break;
    case ScanEventKind::h_display_start:
        m.io.screen_w = (std::min)(512, int((s.active.hdr & 127) + 1) * 8);
        m.vdc.sprite_fetch_remaining = 0;
        break;
    case ScanEventKind::h_front_start: {
        constexpr unsigned costs[] = {4, 4, 8, 16};
        if (m.vdc_mode == VdcMode::accurate && (s.active.control & 0x40))
            m.vdc.sprite_fetch_remaining = static_cast<std::uint16_t>(
                m.vdc.sprite_fetch_cells * costs[(s.active.memory >> 2) & 3]);
        break;
    }
    case ScanEventKind::raster_match:
        if (m.io.vdc[CR].value & 4)
            raise(VDC_RasHit);
        break;
    case ScanEventKind::vblank_irq:
        if (m.io.vdc[CR].value & 8)
            raise(VDC_InVBlank);
        break;
    case ScanEventKind::display_line: {
        m.scanline = e.row;
        const auto plan = ((s.active.control & 0x40) || (m.io.vdc[CR].value & 3))
                              ? EvaluateMachineSprites(m, e.row)
                              : SpriteLinePlan{};
        m.vdc.sprite_fetch_cells = m.vdc_mode == VdcMode::accurate && (s.active.control & 0x40)
                                       ? static_cast<std::uint8_t>(plan.count)
                                       : 0;
        if ((m.io.vdc[CR].value & 2) && plan.overflow)
            raise(VDC_Over);
        if ((m.io.vdc[CR].value & 1) &&
            SpritesCollide(m, plan,
                           {e.row, 0, int(s.active.vdw), int((s.active.hdr & 127) + 1) * 8}))
            raise(VDC_SpHit);
        RenderMachineScanline(m, m.video_surface, e.row);
        break;
    }
    default:
        break;
    }
    if (m.connections.scan_event)
        m.connections.scan_event(m.connections.context, e, m.vce_clock.state().master_time);
}
std::uint64_t next_master_event(const Machine& m) {
    auto next = m.vce_clock.next_event_time();
    if (const auto dot = m.vdc.scan.next_event_dot()) {
        constexpr unsigned divs[] = {4, 3, 2, 2};
        const auto& v = m.vce_clock.state();
        const auto delta = (*dot - v.dots) * divs[v.control & 3] - v.dot_remainder;
        if (delta <= UINT64_MAX - v.master_time)
            next = (std::min)(next, v.master_time + delta);
    }
    return next;
}
} // namespace
void WriteMachineVdcRegister(Machine& m, unsigned reg) {
    if (reg >= CR && reg <= VCR)
        m.vdc.scan.write(static_cast<ScanRegister>(reg - CR), m.io.vdc[reg].value);
}
void ResetMachineVdc(Machine& m) {
    m.video_clock = CpuVideoClock(m.cpu_clock_hz);
    m.vce_clock = VceClock{};
    m.frame_lines = m.vce_clock.state().frame_lines;
    m.line_period = static_cast<int>((std::uint64_t(m.cpu_clock_hz) * 1365 + video_master_hz - 1) /
                                     video_master_hz);
    m.vdc = VdcDevice{};
    ScanRequested r;
    r.control = m.io.vdc[CR].value;
    r.raster = m.io.vdc[RCR].value;
    r.x_scroll = m.io.vdc[BXR].value;
    r.y_scroll = m.io.vdc[BYR].value;
    r.memory = m.io.vdc[MWR].value;
    r.hsr = m.io.vdc[HSR].value;
    r.hdr = m.io.vdc[HDR].value;
    r.vpr = m.io.vdc[VPR].value;
    r.vdw = m.io.vdc[VDW].value;
    r.vcr = m.io.vdc[VCR].value;
    m.vdc.scan = VdcScan(r);
    m.scanline = -1;
    m.io.screen_w = 8;
    m.io.screen_h = 1;
    m.io.minline = m.io.maxline = 0;
    m.io.bg_w = 32 << ((r.memory >> 4) & 3);
    if (m.io.bg_w > 128)
        m.io.bg_w = 128;
    m.io.bg_h = (r.memory & 64) ? 64 : 32;
}
std::uint64_t NextMachineLine(const Machine& m) {
    return m.video_clock.cpu_deadline(next_master_event(m));
}
void AdvanceMachineVdc(Machine& m, std::uint64_t cpu_target) {
    if (m.video_clock.cpu_hz() != m.cpu_clock_hz)
        throw std::invalid_argument("Machine video clock changed without reset");
    m.video_clock.advance_to(cpu_target);
    const auto target = m.video_clock.state().master_time;
    const auto cb = callbacks(m);
    while (m.vce_clock.state().master_time < target) {
        const auto before = m.vce_clock.state();
        const bool low = m.vce_clock.vsync_low();
        auto next = (std::min)(target, next_master_event(m));
        const bool transfer = m.vdc_mode == VdcMode::accurate &&
                              (m.vdc.dma.busy() || m.vdc.cpu.kind != CpuVramKind::none);
        if (transfer) {
            constexpr unsigned divs[] = {4, 3, 2, 2};
            const auto step = divs[before.control & 3] - before.dot_remainder;
            if (step <= UINT64_MAX - before.master_time)
                next = (std::min)(next, before.master_time + step);
        }
        m.vce_clock.advance_to(next);
        const auto& after = m.vce_clock.state();
        const bool edge_dot = after.dots > before.dots && after.dot_remainder == 0;
        if (!transfer)
            m.vdc.dma.advance_to(after.dots - (edge_dot ? 1 : 0), m.vdc.scan.dma_window_open(), cb);
        // Consume the old reservation before events can start a new Hfront budget.
        const auto prior_dots = after.dots - before.dots - (edge_dot ? 1 : 0);
        m.vdc.sprite_fetch_remaining -= static_cast<std::uint16_t>(
            (std::min)(prior_dots, std::uint64_t(m.vdc.sprite_fetch_remaining)));
        m.vdc.scan.advance_to(after.dots, scan_event, &m);
        if (after.line_master == 0)
            m.vdc.scan.begin_line(scan_event, &m);
        if (!low && m.vce_clock.vsync_low())
            m.vdc.scan.vertical_sync(scan_event, &m);
        m.frame_lines = after.frame_lines;
        if (after.frame_number != before.frame_number) {
            m.frame_boundary_pending = true;
            PresentMachineFrame(m, m.video_ports, m.video_surface, after.master_time);
        }
        const bool window = m.vdc.scan.dma_window_open();
        const bool occupied = window && m.vdc.dma.busy();
        m.vdc.dma.advance_to(after.dots, window, cb);
        if (m.vdc_mode == VdcMode::accurate && edge_dot) {
            if (!occupied)
                service_cpu(m);
            if (m.vdc.sprite_fetch_remaining)
                --m.vdc.sprite_fetch_remaining;
        }
        m.scanline = m.vdc.scan.state().row;
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
