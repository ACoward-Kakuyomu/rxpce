#pragma once
#include <cstdint>

namespace rxpce_machine {
// Cache nibble order matches the legacy renderer: 3,7,2,6,1,5,0,4
// from most to least significant. All shifts operate on unsigned 32-bit data.
inline std::uint32_t plane_pixels(std::uint8_t p0, std::uint8_t p1, std::uint8_t p2,
                                  std::uint8_t p3) noexcept {
    std::uint32_t m = p0;
    auto bits = ((m & 0x88) >> 3) | ((m & 0x44) << 6) | ((m & 0x22) << 15) | ((m & 0x11) << 24);
    m = p1;
    bits |= ((m & 0x88) >> 2) | ((m & 0x44) << 7) | ((m & 0x22) << 16) | ((m & 0x11) << 25);
    m = p2;
    bits |= ((m & 0x88) >> 1) | ((m & 0x44) << 8) | ((m & 0x22) << 17) | ((m & 0x11) << 26);
    m = p3;
    bits |= (m & 0x88) | ((m & 0x44) << 9) | ((m & 0x22) << 18) | ((m & 0x11) << 27);
    return bits;
}
// The caller supplies one complete 32-byte background tile and 8 cache words.
inline void background_pixels(const std::uint8_t* tile, std::uint32_t* output) noexcept {
    for (unsigned row = 0; row < 8; ++row) {
        const auto* bytes = tile + row * 2;
        output[row] = plane_pixels(bytes[0], bytes[1], bytes[16], bytes[17]);
    }
}
// The caller supplies one complete 128-byte sprite tile and 32 cache words.
inline void sprite_pixels(const std::uint8_t* tile, std::uint32_t* output) noexcept {
    for (unsigned byte = 0; byte < 32; ++byte)
        output[byte] = plane_pixels(tile[byte], tile[byte + 32], tile[byte + 64], tile[byte + 96]);
}
} // namespace rxpce_machine
