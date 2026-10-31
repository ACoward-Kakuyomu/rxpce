#include "vce_clock.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace rxpce_video {
namespace {
constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
std::uint64_t checked_add(std::uint64_t a, std::uint64_t b) {
    if (b > maximum - a)
        throw std::overflow_error("Video clock addition overflow");
    return a + b;
}
std::uint64_t checked_multiply(std::uint64_t a, std::uint32_t b) {
    if (a > maximum / b)
        throw std::overflow_error("Video clock multiplication overflow");
    return a * b;
}
CpuVideoState convert(std::uint64_t cpu, std::uint32_t hz) {
    // The small product is bounded by UINT32_MAX * 21477270, below UINT64_MAX.
    const auto small = (cpu % hz) * video_master_hz;
    return {cpu, checked_add(checked_multiply(cpu / hz, video_master_hz), small / hz),
            static_cast<std::uint32_t>(small % hz)};
}
unsigned divisor(std::uint8_t control) {
    constexpr unsigned divs[] = {4, 3, 2, 2};
    return divs[control & 3];
}
} // namespace
CpuVideoClock::CpuVideoClock(std::uint32_t hz) : cpu_hz_(hz) {
    if (!hz)
        throw std::invalid_argument("CPU clock must be positive");
}
void CpuVideoClock::advance_to(std::uint64_t cpu_time) {
    if (cpu_time < state_.cpu_time)
        throw std::invalid_argument("CPU video clock moved backwards");
    // Absolute quotient/remainder is also independent of call partitioning.
    const auto next = convert(cpu_time, cpu_hz_);
    state_ = next;
}
std::uint64_t CpuVideoClock::cpu_deadline(std::uint64_t master_time) const {
    const auto small = (master_time % video_master_hz) * cpu_hz_;
    return checked_add(checked_multiply(master_time / video_master_hz, cpu_hz_),
                       small / video_master_hz + (small % video_master_hz != 0));
}
void CpuVideoClock::restore(const CpuVideoState& s) {
    const auto expected = convert(s.cpu_time, cpu_hz_);
    if (s.remainder != expected.remainder || s.master_time != expected.master_time)
        throw std::invalid_argument("Inconsistent CPU video clock phase");
    state_ = s;
}
VceClock::VceClock(std::uint8_t control) {
    state_.control = control;
    state_.frame_lines = (control & 4) ? 263 : 262;
}
unsigned VceClock::divider() const noexcept {
    return divisor(state_.control);
}
void VceClock::write_control(std::uint8_t control) noexcept {
    if (divisor(control) != divider())
        state_.dot_remainder = 0;
    state_.control = control;
}
bool VceClock::hsync_low() const noexcept {
    return state_.line_master >= 1128;
}
bool VceClock::vsync_low() const noexcept {
    const unsigned phase = state_.line * vce_line_ticks + state_.line_master;
    return phase >= (state_.frame_lines - 4) * vce_line_ticks + 1158 &&
           phase < (state_.frame_lines - 1) * vce_line_ticks + 1158;
}
unsigned VceClock::next_offset() const noexcept {
    if (state_.line_master < 1128)
        return 1128;
    if (state_.line_master < 1158 &&
        (state_.line == state_.frame_lines - 4 || state_.line == state_.frame_lines - 1))
        return 1158;
    return vce_line_ticks;
}
std::uint64_t VceClock::next_event_time() const {
    return checked_add(state_.master_time, next_offset() - state_.line_master);
}
std::uint64_t VceClock::next_line_time() const {
    return checked_add(state_.master_time, vce_line_ticks - state_.line_master);
}
void VceClock::add_ticks(std::uint64_t delta) {
    const auto div = divider();
    const auto small = delta % div + state_.dot_remainder;
    const auto dots = checked_add(state_.dots, delta / div + small / div);
    const auto time = checked_add(state_.master_time, delta);
    state_.dots = dots;
    state_.master_time = time;
    state_.dot_remainder = static_cast<std::uint8_t>(small % div);
}
void VceClock::advance_to(std::uint64_t target, Sink sink, void* context) {
    if (target < state_.master_time)
        throw std::invalid_argument("VCE clock moved backwards");
    auto emit = [&](VceEventKind kind) {
        if (sink)
            sink(context, {kind, state_.master_time, state_.frame_number, state_.line});
    };
    while (state_.master_time < target) {
        const auto requested = (state_.control & 4) ? 263 : 262;
        if (!sink && !state_.line && !state_.line_master && requested == state_.frame_lines) {
            const std::uint64_t length = state_.frame_lines * vce_line_ticks;
            const auto frames = (target - state_.master_time) / length;
            if (frames) {
                const auto next_frame = checked_add(state_.frame_number, frames);
                add_ticks(frames * length); // Bounded by target-master_time.
                state_.frame_number = next_frame;
                continue;
            }
        }
        const auto end = next_offset();
        const auto delta =
            (std::min)(target - state_.master_time, std::uint64_t(end - state_.line_master));
        add_ticks(delta);
        state_.line_master += static_cast<std::uint16_t>(delta);
        if (state_.line_master != end)
            continue;
        if (end == 1128)
            emit(VceEventKind::hsync_assert);
        else if (end == 1158)
            emit(state_.line == state_.frame_lines - 4 ? VceEventKind::vsync_assert
                                                       : VceEventKind::vsync_release);
        else {
            state_.line_master = 0;
            const bool wrap = ++state_.line == state_.frame_lines;
            if (wrap) {
                state_.line = 0;
                ++state_.frame_number;
                state_.frame_lines = static_cast<std::uint16_t>(requested);
            }
            emit(VceEventKind::line_start);
            if (wrap)
                emit(VceEventKind::frame_start);
        }
    }
}
void VceClock::restore(const VceClockState& s) {
    if ((s.frame_lines != 262 && s.frame_lines != 263) || s.line >= s.frame_lines ||
        s.line_master >= vce_line_ticks || s.master_time % vce_line_ticks != s.line_master ||
        s.master_time / vce_line_ticks < s.line || s.dot_remainder >= divisor(s.control) ||
        s.dot_remainder > s.master_time || s.dots > (s.master_time - s.dot_remainder) / 2)
        throw std::invalid_argument("Invalid VCE clock phase");
    const auto completed = s.master_time / vce_line_ticks - s.line;
    if (s.frame_number > completed / 262 ||
        s.frame_number < completed / 263 + (completed % 263 != 0))
        throw std::invalid_argument("Invalid VCE frame history");
    state_ = s;
}
} // namespace rxpce_video
