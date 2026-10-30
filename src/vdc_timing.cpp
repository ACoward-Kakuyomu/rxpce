#include "vdc_timing.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace rxpce_video {
unsigned VdcTimingState::divider() const noexcept {
    constexpr unsigned divisors[] = {4, 3, 2, 2};
    return divisors[vce_control & 3];
}
VdcTiming::VdcTiming(VdcTimingConfig config, VdcTimingRegisters initial, std::uint64_t start_time)
    : config_(config) {
    if (!config.line_period || !config.frame_lines || config.first_active > config.last_active ||
        config.last_active >= config.frame_lines - 1)
        throw std::invalid_argument("Invalid VDC timing geometry");
    state_.cpu_time = start_time;
    state_.vce_control = initial.vce_control;
    state_.requested_cr = initial.cr;
    state_.line_cr = initial.cr & 0xc0;
    state_.requested_mwr = state_.active_mwr = initial.mwr;
    state_.burst = (initial.cr & 0xc0) == 0;
}
std::uint64_t VdcTiming::next_line_time() const {
    const auto remaining = config_.line_period - state_.line_offset;
    if (state_.cpu_time > std::numeric_limits<std::uint64_t>::max() - remaining)
        throw std::overflow_error("VDC deadline overflow");
    return state_.cpu_time + remaining;
}
void VdcTiming::write_vce(std::uint8_t value) noexcept {
    const auto old_divider = state_.divider();
    state_.vce_control = value;
    // Explicit approximation: changing the effective divider discards a partial dot.
    if (state_.divider() != old_divider)
        state_.dot_remainder = 0;
}
bool VdcTiming::dma_window_open() const noexcept {
    return state_.burst || state_.line < config_.first_active || state_.line > config_.last_active;
}
void VdcTiming::restore(const VdcTimingState& s) {
    if (s.line >= config_.frame_lines || s.line_offset >= config_.line_period ||
        s.dot_remainder >= s.divider() || (s.line_cr & ~0xc0) || s.cpu_time < s.line_offset ||
        s.line_dots > std::uint64_t(s.line_offset) * 3 / 2 + 1 || (!s.line_offset && s.line_dots))
        throw std::invalid_argument("Invalid VDC clock state");
    state_ = s;
}
void VdcTiming::set_display_range(std::uint32_t first, std::uint32_t last) {
    if (first > last || last >= config_.frame_lines - 1)
        throw std::invalid_argument("Invalid VDC display range");
    config_.first_active = first;
    config_.last_active = last;
}
void VdcTiming::advance_to(std::uint64_t time, BoundarySink sink, void* context) {
    if (time < state_.cpu_time)
        throw std::invalid_argument("VDC time moved backwards");
    while (state_.cpu_time < time) {
        const auto delta = std::min<std::uint64_t>(time - state_.cpu_time,
                                                   config_.line_period - state_.line_offset);
        // delta is bounded by a 32-bit line period, so the master-tick product cannot overflow.
        const auto master = delta * 3 + state_.dot_remainder;
        const auto dots = master / state_.divider();
        if (dots > std::numeric_limits<std::uint64_t>::max() - state_.dots)
            throw std::overflow_error("VDC dot count overflow");
        state_.dots += dots;
        state_.line_dots += dots;
        state_.dot_remainder = static_cast<std::uint8_t>(master % state_.divider());
        state_.cpu_time += delta;
        state_.line_offset += static_cast<std::uint32_t>(delta);
        if (state_.line_offset != config_.line_period)
            continue;
        state_.line_offset = 0;
        state_.line_dots = 0;
        state_.line = (state_.line + 1) % config_.frame_lines;
        const VdcBoundary boundary{state_.line == config_.first_active,
                                   state_.line == config_.last_active + 1, state_.line == 0};
        state_.line_cr = state_.requested_cr & 0xc0;
        if (boundary.display_start)
            state_.burst = (state_.requested_cr & 0xc0) == 0;
        if (boundary.vblank_start)
            state_.active_mwr = state_.requested_mwr;
        if (sink)
            sink(context, state_, boundary);
    }
}
} // namespace rxpce_video
