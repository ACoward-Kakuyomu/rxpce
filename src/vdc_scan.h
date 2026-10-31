#pragma once
#include <cstdint>
#include <optional>
namespace rxpce_video {
enum class HPeriod : std::uint8_t { sync, back, display, front, waiting };
enum class VPeriod : std::uint8_t { sync, back, display, front };
enum class ScanRegister {
    control,
    raster,
    x_scroll,
    y_scroll,
    memory,
    horizontal_sync,
    horizontal_display,
    vertical_sync,
    vertical_display,
    vertical_end
};
struct ScanRequested {
    std::uint16_t control = 0, raster = 0, x_scroll = 0, y_scroll = 0, memory = 0;
    std::uint16_t hsr = 0, hdr = 0, vpr = 0, vdw = 0, vcr = 0;
};
struct ScanLatches {
    std::uint16_t control = 0, x_scroll = 0, y_scroll = 0, memory = 0;
    std::uint16_t hsr = 0, hdr = 0, vpr = 0, vdw = 0, vcr = 0;
};
struct VdcScanState {
    ScanRequested requested;
    ScanLatches active;
    std::uint64_t dot_time = 0, physical_line = 0, last_line_dot = 0;
    HPeriod h_phase = HPeriod::sync;
    VPeriod v_phase = VPeriod::sync;
    std::uint16_t h_remaining = 8, v_remaining = 1, raster = 0, y_counter = 0;
    std::int16_t row = -1;
    bool burst = true, y_pending = false, vblank_pending = false;
};
enum class ScanEventKind {
    hsync_start,
    h_back_start,
    h_display_start,
    h_front_start,
    vertical_sync_start,
    vertical_back_start,
    display_start,
    vblank_start,
    raster_match,
    vblank_irq,
    display_line
};
struct ScanEvent {
    ScanEventKind kind;
    std::uint64_t dot_time, physical_line;
    std::int16_t row;
    std::uint16_t raster;
};
// External-sync candidate model. See vce_timing_a8.md for the adopted approximations.
class VdcScan {
  public:
    using Sink = void (*)(void*, const ScanEvent&);
    explicit VdcScan(ScanRequested initial = {});
    const VdcScanState& state() const noexcept {
        return state_;
    }
    void write(ScanRegister, std::uint16_t);
    void advance_to(std::uint64_t dot_time, Sink = nullptr, void* context = nullptr);
    // First advance to the external edge, then signal it. Sinks may not reenter this model.
    void begin_line(Sink = nullptr, void* context = nullptr);
    void vertical_sync(Sink = nullptr, void* context = nullptr);
    std::optional<std::uint64_t> next_event_dot() const;
    bool dma_window_open() const noexcept;
    void restore(const VdcScanState&);

  private:
    void emit(ScanEventKind, Sink, void*);
    void enter_horizontal(HPeriod, Sink, void*);
    void enter_vertical(VPeriod, Sink, void*);
    VdcScanState state_;
};
} // namespace rxpce_video
