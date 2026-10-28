#pragma once
#include <cstdint>
#include <array>

namespace rxpce_video {
inline void transfer_satb(const std::uint8_t* vram, std::array<std::int16_t, 256>& sprites,
                          std::uint16_t source) {
    for (auto& sprite : sprites) {
        const unsigned offset = static_cast<unsigned>(source) * 2;
        const unsigned value = vram[offset] | (unsigned(vram[offset + 1]) << 8);
        sprite = static_cast<std::int16_t>(value <= 0x7fff ? static_cast<int>(value)
                                                           : static_cast<int>(value) - 0x10000);
        source = static_cast<std::uint16_t>(source + 1);
    }
}
// Addresses and LENR are 16-bit word registers. The caller supplies the full
// address space and one dirty byte per background/sprite pattern (32/128 bytes).
inline void transfer_vram(std::uint8_t* vram, std::uint8_t* background_dirty,
                          std::uint8_t* sprite_dirty, std::uint16_t& source,
                          std::uint16_t& destination, std::uint16_t& length,
                          std::uint16_t control) {
    const int source_step = (control & 4) ? -1 : 1;
    const int destination_step = (control & 8) ? -1 : 1;
    const unsigned count = static_cast<unsigned>(length) + 1;
    for (unsigned i = 0; i < count; ++i) {
        const unsigned from = static_cast<unsigned>(source) * 2;
        const unsigned to = static_cast<unsigned>(destination) * 2;
        const auto low = vram[from], high = vram[from + 1];
        vram[to] = low;
        vram[to + 1] = high;
        background_dirty[destination / 16] = 1;
        sprite_dirty[destination / 64] = 1;
        source = static_cast<std::uint16_t>(source + source_step);
        destination = static_cast<std::uint16_t>(destination + destination_step);
    }
    length = 0xffff;
}
} // namespace rxpce_video
