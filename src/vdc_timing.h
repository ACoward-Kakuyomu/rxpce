#pragma once
#include <cstdint>

namespace rxpce_video {
// CPU high-speed clock units. The legacy host line period is deliberately retained.
struct VdcTimingConfig {
    std::uint32_t line_period = 454, frame_lines = 263, first_active = 0, last_active = 231;
};
struct VdcTimingRegisters {
    std::uint8_t vce_control = 0;
    std::uint16_t cr = 0, mwr = 0;
};
struct VdcTimingState {
    std::uint64_t cpu_time = 0, dots = 0;
    std::uint64_t line_dots = 0;
    std::uint32_t line_offset = 0, line = 0;
    std::uint8_t dot_remainder = 0, vce_control = 0;
    std::uint16_t requested_cr = 0, line_cr = 0; // line_cr holds only the display bits ($c0).
    std::uint16_t requested_mwr = 0, active_mwr = 0;
    bool burst = true;
    unsigned divider() const noexcept;
};
struct VdcBoundary {
    bool display_start = false, vblank_start = false, frame_wrap = false;
};
// Model-only foundation for incremental DMA; not yet connected to native Machine.
class VdcTiming {
  public:
    using BoundarySink = void (*)(void*, const VdcTimingState&, VdcBoundary);
    explicit VdcTiming(VdcTimingConfig config = {}, VdcTimingRegisters initial = {},
                       std::uint64_t start_time = 0);
    const VdcTimingState& state() const noexcept {
        return state_;
    }
    const VdcTimingConfig& config() const noexcept {
        return config_;
    }
    void advance_to(std::uint64_t cpu_time, BoundarySink sink = nullptr, void* context = nullptr);
    std::uint64_t next_line_time() const;
    void write_vce(std::uint8_t value) noexcept;
    void write_cr(std::uint16_t value) noexcept {
        state_.requested_cr = value;
    }
    void write_mwr(std::uint16_t value) noexcept {
        state_.requested_mwr = value;
    }
    bool dma_window_open() const noexcept;
    void restore(const VdcTimingState&);
    void set_display_range(std::uint32_t first, std::uint32_t last);

  private:
    VdcTimingConfig config_;
    VdcTimingState state_;
};
} // namespace rxpce_video
