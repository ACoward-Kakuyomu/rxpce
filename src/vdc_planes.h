#pragma once
#include <cstdint>

namespace rxpce_video {
// Selection of source planes and their positions in the output color nibble.
// HuC6270 H7-18 (CG) and H7-24 (SG): only SG's upper pair shifts down.
struct PlaneSelection {
    unsigned first = 0, count = 4, shift = 0;
    std::uint32_t packed(std::uint32_t dots) const noexcept {
        const auto mask = ((1u << count) - 1) << first;
        return (dots & (mask * 0x11111111u)) >> shift;
    }
};
inline PlaneSelection BackgroundPlanes(std::uint16_t memory) noexcept {
    if ((memory & 3) != 3)
        return {};
    return {memory & 0x80 ? 2u : 0u, 2, 0};
}
inline PlaneSelection SpritePlanes(std::uint16_t memory, unsigned code) noexcept {
    if ((memory & 12) != 4)
        return {};
    const auto first = (code & 1) * 2;
    return {first, 2, first};
}
} // namespace rxpce_video
