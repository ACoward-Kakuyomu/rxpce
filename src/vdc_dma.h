#pragma once
#include <cstdint>

namespace rxpce_video {
enum class DmaChannel : std::uint8_t { vram, satb };
enum class DmaPhase : std::uint8_t { idle, requested, read, write };
struct VdcDmaState {
    std::uint64_t dot_time = 0;
    DmaPhase phase = DmaPhase::idle;
    std::uint16_t source = 0, destination = 0;
    std::uint32_t remaining = 0;
    std::uint8_t direction = 0;
    std::uint16_t read_latch = 0;
    std::uint8_t elapsed = 0;
    bool satb_requested = false, satb_running = false;
    std::uint16_t satb_request_source = 0, satb_source = 0, satb_index = 0;
    std::uint8_t satb_elapsed = 0;
};
struct DmaCallbacks {
    // Callbacks are invoked only at a completed bus slot.
    void* context;
    std::uint16_t (*read)(void*, std::uint16_t, std::uint64_t);
    void (*write)(void*, std::uint16_t, std::uint16_t, std::uint64_t);
    void (*satb_write)(void*, unsigned, std::uint16_t, std::uint64_t);
    void (*progress)(void*, std::uint16_t, std::uint16_t, std::uint16_t);
    void (*complete)(void*, DmaChannel, std::uint64_t);
};
enum class CpuVramKind : std::uint8_t { none, read, write };
struct CpuVramAccess {
    CpuVramKind kind = CpuVramKind::none;
    std::uint16_t address = 0, value = 0, read_buffer = 0;
    std::uint8_t elapsed = 0;
    std::uint16_t increment = 1;
};
bool valid_dma(const VdcDmaState&) noexcept;
class VdcDma {
  public:
    const VdcDmaState& state() const noexcept {
        return state_;
    }
    void restore(const VdcDmaState&);
    void request_vram(std::uint16_t source, std::uint16_t destination, std::uint16_t length,
                      std::uint16_t control) noexcept;
    void request_satb(std::uint16_t source) noexcept;
    void vblank_start(bool repeat, std::uint16_t source) noexcept;
    void display_start(bool burst) noexcept;
    void advance_to(std::uint64_t dots, bool window_open, const DmaCallbacks&);
    bool busy() const noexcept {
        return state_.phase != DmaPhase::idle || state_.satb_running;
    }

  private:
    VdcDmaState state_;
};
} // namespace rxpce_video
