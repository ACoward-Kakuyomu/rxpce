#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace rxpce_cd::detail {
inline std::uint32_t edc(const unsigned char* bytes, std::size_t size) {
    static const auto table = [] {
        std::array<std::uint32_t, 256> result{};
        std::uint32_t seed = 0;
        for (auto& entry : result) {
            auto value = seed++;
            for (unsigned bit = 0; bit < 8; ++bit)
                value = (value >> 1) ^ ((value & 1) ? 0xd8018001u : 0u);
            entry = value;
        }
        return result;
    }();
    std::uint32_t result = 0;
    for (std::size_t n = 0; n < size; ++n)
        result = (result >> 8) ^ table[(result ^ bytes[n]) & 255];
    return result;
}
} // namespace rxpce_cd::detail
