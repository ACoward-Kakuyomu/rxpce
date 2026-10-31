#include "vdc_scan.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace rxpce_video {
namespace {
unsigned horizontal_length(const VdcScanState& s) {
    switch (s.h_phase) {
    case HPeriod::sync:
        return ((s.active.hsr & 31) + 1) * 8;
    case HPeriod::back:
        return (((s.active.hsr >> 8) & 127) + 1) * 8;
    case HPeriod::display:
        return ((s.active.hdr & 127) + 1) * 8;
    case HPeriod::front:
        return (((s.active.hdr >> 8) & 127) + 1) * 8;
    default:
        return 0;
    }
}
unsigned vertical_length(const VdcScanState& s) {
    switch (s.v_phase) {
    case VPeriod::sync:
        return (s.active.vpr & 31) + 1;
    case VPeriod::back:
        return (s.active.vpr >> 8) + 2;
    case VPeriod::display:
        return s.active.vdw + 1;
    case VPeriod::front:
        return s.active.vcr;
    }
    return 0;
}
} // namespace
VdcScan::VdcScan(ScanRequested r) {
    state_.requested = r;
    state_.active.memory = r.memory;
    enter_vertical(VPeriod::sync, nullptr, nullptr);
    enter_horizontal(HPeriod::sync, nullptr, nullptr);
}
void VdcScan::emit(ScanEventKind kind, Sink sink, void* context) {
    if (sink)
        sink(context, {kind, state_.dot_time, state_.physical_line, state_.row, state_.raster});
}
void VdcScan::write(ScanRegister reg, std::uint16_t value) {
    auto& r = state_.requested;
    switch (reg) {
    case ScanRegister::control:
        r.control = value;
        break;
    case ScanRegister::raster:
        r.raster = value;
        break;
    case ScanRegister::x_scroll:
        r.x_scroll = value;
        break;
    case ScanRegister::y_scroll:
        r.y_scroll = value;
        state_.y_pending = true;
        break;
    case ScanRegister::memory:
        r.memory = value;
        break;
    case ScanRegister::horizontal_sync:
        r.hsr = value;
        break;
    case ScanRegister::horizontal_display:
        r.hdr = value;
        break;
    case ScanRegister::vertical_sync:
        r.vpr = value;
        break;
    case ScanRegister::vertical_display:
        r.vdw = value;
        break;
    case ScanRegister::vertical_end:
        r.vcr = value;
        break;
    default:
        throw std::invalid_argument("Invalid VDC scan register");
    }
}
void VdcScan::enter_horizontal(HPeriod period, Sink sink, void* context) {
    auto& s = state_;
    const auto& r = s.requested;
    s.h_phase = period;
    auto kind = ScanEventKind::hsync_start;
    switch (period) {
    case HPeriod::sync:
        s.active.hsr = (s.active.hsr & 0x7f00) | (r.hsr & 31);
        break;
    case HPeriod::back:
        s.active.hsr = (s.active.hsr & 31) | (r.hsr & 0x7f00);
        kind = ScanEventKind::h_back_start;
        break;
    case HPeriod::display:
        s.active.hdr = (s.active.hdr & 0x7f00) | (r.hdr & 127);
        s.active.control = r.control & 0xc0;
        s.active.x_scroll = r.x_scroll & 1023;
        if (s.v_phase == VPeriod::display) {
            if (s.row == 0)
                s.y_counter = r.y_scroll & 511;
            else if (s.y_pending)
                s.y_counter = (r.y_scroll + 1) & 511;
            else
                s.y_counter = (s.y_counter + 1) & 511;
            s.active.y_scroll = r.y_scroll & 511;
            s.y_pending = false;
        }
        kind = ScanEventKind::h_display_start;
        break;
    case HPeriod::front:
        s.active.hdr = (s.active.hdr & 127) | (r.hdr & 0x7f00);
        kind = ScanEventKind::h_front_start;
        break;
    case HPeriod::waiting:
        break;
    }
    s.h_remaining = static_cast<std::uint16_t>(horizontal_length(s));
    if (period != HPeriod::waiting)
        emit(kind, sink, context);
    if (period == HPeriod::display) {
        if (s.vblank_pending) {
            s.vblank_pending = false;
            emit(ScanEventKind::vblank_irq, sink, context);
        }
        if (s.raster == (r.raster & 1023))
            emit(ScanEventKind::raster_match, sink, context);
        if (s.v_phase == VPeriod::display)
            emit(ScanEventKind::display_line, sink, context);
    }
}
void VdcScan::enter_vertical(VPeriod period, Sink sink, void* context) {
    auto& s = state_;
    const auto& r = s.requested;
    s.v_phase = period;
    s.row = -1;
    auto kind = ScanEventKind::vertical_sync_start;
    switch (period) {
    case VPeriod::sync:
        s.active.vpr = (s.active.vpr & 0xff00) | (r.vpr & 31);
        break;
    case VPeriod::back:
        s.active.vpr = (s.active.vpr & 31) | (r.vpr & 0xff00);
        kind = ScanEventKind::vertical_back_start;
        break;
    case VPeriod::display:
        s.active.vdw = r.vdw & 511;
        s.row = 0;
        s.burst = (r.control & 0xc0) == 0;
        kind = ScanEventKind::display_start;
        break;
    case VPeriod::front:
        s.active.vcr = r.vcr & 255;
        s.active.memory = r.memory;
        s.vblank_pending = true;
        kind = ScanEventKind::vblank_start;
        break;
    }
    s.v_remaining = static_cast<std::uint16_t>(vertical_length(s));
    // A zero front porch has no restorable intermediate state. Commit the next
    // nonempty period before publishing both simultaneous boundary events.
    const bool zero_front = s.v_remaining == 0;
    if (zero_front) {
        s.v_phase = VPeriod::sync;
        s.active.vpr = (s.active.vpr & 0xff00) | (r.vpr & 31);
        s.v_remaining = static_cast<std::uint16_t>(vertical_length(s));
    }
    emit(kind, sink, context);
    if (zero_front)
        emit(ScanEventKind::vertical_sync_start, sink, context);
}
void VdcScan::begin_line(Sink sink, void* context) {
    auto& s = state_;
    if (s.dot_time <= s.last_line_dot ||
        s.physical_line == (std::numeric_limits<std::uint64_t>::max)())
        throw std::invalid_argument("Invalid external VDC line edge");
    s.last_line_dot = s.dot_time;
    ++s.physical_line;
    enter_horizontal(HPeriod::sync, sink, context);
    s.raster = (s.raster + 1) & 1023;
    if (s.v_phase == VPeriod::back && s.v_remaining == 2)
        s.raster = 64;
    if (--s.v_remaining == 0) {
        do {
            const auto next = static_cast<VPeriod>((static_cast<unsigned>(s.v_phase) + 1) % 4);
            enter_vertical(next, sink, context);
        } while (!s.v_remaining);
    } else if (s.v_phase == VPeriod::display)
        ++s.row;
}
void VdcScan::vertical_sync(Sink sink, void* context) {
    const bool ended = state_.v_phase == VPeriod::display;
    if (ended)
        enter_vertical(VPeriod::front, sink, context);
    if (state_.vblank_pending) {
        state_.vblank_pending = false;
        emit(ScanEventKind::vblank_irq, sink, context);
    }
    if (!ended || state_.v_phase != VPeriod::sync)
        enter_vertical(VPeriod::sync, sink, context);
}
bool VdcScan::dma_window_open() const noexcept {
    return state_.burst || state_.v_phase != VPeriod::display;
}
std::optional<std::uint64_t> VdcScan::next_event_dot() const {
    if (state_.h_phase == HPeriod::waiting)
        return std::nullopt;
    if (state_.dot_time > (std::numeric_limits<std::uint64_t>::max)() - state_.h_remaining)
        throw std::overflow_error("VDC scan deadline overflow");
    return state_.dot_time + state_.h_remaining;
}
void VdcScan::advance_to(std::uint64_t target, Sink sink, void* context) {
    auto& s = state_;
    if (target < s.dot_time)
        throw std::invalid_argument("VDC scan moved backwards");
    while (s.dot_time < target) {
        if (s.h_phase == HPeriod::waiting) {
            s.dot_time = target;
            break;
        }
        const auto delta = (std::min)(target - s.dot_time, std::uint64_t(s.h_remaining));
        s.dot_time += delta;
        s.h_remaining -= static_cast<std::uint16_t>(delta);
        if (!s.h_remaining)
            enter_horizontal(static_cast<HPeriod>(static_cast<unsigned>(s.h_phase) + 1), sink,
                             context);
    }
}
void VdcScan::restore(const VdcScanState& s) {
    const auto& a = s.active;
    if (static_cast<unsigned>(s.h_phase) > 4 || static_cast<unsigned>(s.v_phase) > 3 ||
        s.last_line_dot > s.dot_time || s.raster > 1023 || s.y_counter > 511 ||
        (a.control & ~0xc0) || (a.hsr & ~0x7f1f) || (a.hdr & ~0x7f7f) || (a.vpr & ~0xff1f) ||
        a.vdw > 511 || a.vcr > 255 || a.x_scroll > 1023 || a.y_scroll > 511 ||
        (s.h_phase == HPeriod::waiting ? s.h_remaining != 0
                                       : !s.h_remaining || s.h_remaining > horizontal_length(s)) ||
        !s.v_remaining || s.v_remaining > vertical_length(s) ||
        (s.v_phase == VPeriod::display ? s.row != int(a.vdw + 1 - s.v_remaining) : s.row != -1))
        throw std::invalid_argument("Invalid VDC scan state");
    const unsigned lengths[] = {
        unsigned((a.hsr & 31) + 1) * 8, unsigned(((a.hsr >> 8) & 127) + 1) * 8,
        unsigned((a.hdr & 127) + 1) * 8, unsigned(((a.hdr >> 8) & 127) + 1) * 8};
    std::uint64_t completed = 0;
    for (unsigned n = 0; n < static_cast<unsigned>(s.h_phase); ++n)
        completed += lengths[n];
    if (s.h_phase != HPeriod::waiting)
        completed += horizontal_length(s) - s.h_remaining;
    const auto elapsed = s.dot_time - s.last_line_dot;
    if ((!s.physical_line && s.last_line_dot) ||
        (s.h_phase == HPeriod::waiting ? elapsed < completed : elapsed != completed))
        throw std::invalid_argument("Inconsistent VDC horizontal phase");
    state_ = s;
}
} // namespace rxpce_video
