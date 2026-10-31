#pragma once
#include <cstdint>

namespace rxpce_video {
inline constexpr std::uint32_t video_master_hz = 21477270;
inline constexpr std::uint32_t vce_line_ticks = 1365;
struct CpuVideoState {
    std::uint64_t cpu_time = 0, master_time = 0;
    std::uint32_t remainder = 0;
};
// A8a independent model. Native Machine/host integration belongs to A8b.
class CpuVideoClock {
  public:
    explicit CpuVideoClock(std::uint32_t cpu_hz);
    const CpuVideoState& state() const noexcept {
        return state_;
    }
    std::uint32_t cpu_hz() const noexcept {
        return cpu_hz_;
    }
    void advance_to(std::uint64_t cpu_time);
    // Absolute ceiling conversion, including past deadlines; does not advance the clock.
    std::uint64_t cpu_deadline(std::uint64_t master_time) const;
    void restore(const CpuVideoState&);

  private:
    std::uint32_t cpu_hz_;
    CpuVideoState state_;
};
enum class VceEventKind { hsync_assert, line_start, frame_start, vsync_assert, vsync_release };
struct VceEvent {
    VceEventKind kind;
    std::uint64_t master_time, frame_number;
    std::uint16_t line;
};
struct VceClockState {
    std::uint64_t master_time = 0, frame_number = 0, dots = 0;
    std::uint16_t line = 0, line_master = 0, frame_lines = 262;
    std::uint8_t control = 0, dot_remainder = 0;
};
class VceClock {
  public:
    using Sink = void (*)(void*, const VceEvent&);
    explicit VceClock(std::uint8_t control = 0);
    const VceClockState& state() const noexcept {
        return state_;
    }
    void write_control(std::uint8_t control) noexcept;
    // Sinks observe completed boundaries; they must not reenter or mutate this clock.
    void advance_to(std::uint64_t master_time, Sink sink = nullptr, void* context = nullptr);
    std::uint64_t next_event_time() const;
    std::uint64_t next_line_time() const;
    bool hsync_low() const noexcept;
    bool vsync_low() const noexcept;
    void restore(const VceClockState&);

  private:
    unsigned divider() const noexcept;
    unsigned next_offset() const noexcept;
    void add_ticks(std::uint64_t delta);
    VceClockState state_;
};
} // namespace rxpce_video
