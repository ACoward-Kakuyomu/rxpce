#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>

namespace rxpce_video {
using SpriteSatb = std::array<std::int16_t, 256>;
struct SpriteLineLimits {
    unsigned search_entries = 64, fetch_cells = 16;
};
struct SpriteCell {
    std::uint8_t satb_index = 0, screen_cell = 0, source_cell = 0, row = 0;
};
struct SpriteLinePlan {
    std::array<SpriteCell, 16> cells{};
    unsigned count = 0, requested_cells = 0;
    bool overflow = false;
};
// Shared scanline model from H7-19; see sprite_evaluation_a7.md for remaining approximations.
inline SpriteLineLimits SpriteLimitsFromRegisters(std::uint16_t hsr, std::uint16_t hdr,
                                                  std::uint16_t mwr) noexcept {
    const unsigned sm = (mwr >> 2) & 3;
    const unsigned d = (hdr & 127) + 1;
    const unsigned e = ((hdr >> 8) & 127) + (hsr & 31) + ((hsr >> 8) & 127) + 3;
    const unsigned available = e - 2; // Minimum e is 3 after register masking.
    return {d >= 32 ? 64 : 2 * d + (sm < 2 ? 1 : 0),
            (std::min)(16u, sm < 2 ? available * 2 : (sm == 2 ? available : available / 2))};
}
// Geometry only: no pixels, host visibility, IRQ state or persistent latches.
inline SpriteLinePlan EvaluateSpriteLine(const SpriteSatb& satb, int line,
                                         SpriteLineLimits limits = {}) {
    if (limits.search_entries > 64 || limits.fetch_cells > 16)
        throw std::invalid_argument("Invalid sprite line limits");
    SpriteLinePlan plan;
    constexpr int heights[] = {16, 32, 64, 64};
    for (unsigned n = 0; n < limits.search_entries; ++n) {
        const unsigned attributes = static_cast<std::uint16_t>(satb[n * 4 + 3]);
        const int top = (satb[n * 4] & 1023) - 64;
        const int height = heights[(attributes >> 12) & 3];
        // Compare bounds before subtracting, including arbitrary caller-supplied lines.
        if (line < top || line >= top + height)
            continue;
        const unsigned width = attributes & 256 ? 2 : 1;
        const unsigned row = attributes & 0x8000 ? height - 1 - (line - top) : line - top;
        plan.requested_cells += width;
        for (unsigned screen = 0; screen < width && plan.count < limits.fetch_cells; ++screen)
            plan.cells[plan.count++] = {
                static_cast<std::uint8_t>(n), static_cast<std::uint8_t>(screen),
                static_cast<std::uint8_t>(attributes & 0x800 ? width - 1 - screen : screen),
                static_cast<std::uint8_t>(row)};
    }
    plan.overflow = plan.requested_cells > limits.fetch_cells;
    return plan;
}
} // namespace rxpce_video
