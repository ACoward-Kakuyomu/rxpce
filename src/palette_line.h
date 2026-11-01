#pragma once
#include "video_frame.h"
#include <array>
#include <cstdint>
#include <stdexcept>
namespace rxpce_machine {
class Machine;
struct FrameSurface;
class PaletteEntry {
  public:
    constexpr PaletteEntry() = default;
    explicit constexpr PaletteEntry(unsigned index) : index_(static_cast<std::uint16_t>(index)) {
        if (index >= 512)
            throw std::invalid_argument("Invalid palette entry");
    }
    constexpr unsigned index() const {
        return index_;
    }

  private:
    std::uint16_t index_ = 0;
};
constexpr unsigned palette_line_pixels = 576;
struct PaletteLine {
    std::array<PaletteEntry, palette_line_pixels> entries{};
    std::array<std::uint64_t, 8> used{};
    std::uint64_t start_master = 0;
    int output_row = -1;
    unsigned width = 0, committed = 0, divider = 4;
    bool monochrome = false, active = false, used_valid = false;
};
// Compose one immutable index row; does not write final display pixels.
void ComposePaletteLine(Machine&, PaletteLine&, int row);
// Finalize [committed, end), in full host-row coordinates including gutters.
void ResolvePaletteLine(PaletteLine&, FrameSurface,
                        const std::array<rxpce_video::Pixel, 512>& palette, unsigned end);
} // namespace rxpce_machine
