#pragma once
#include "vdc_mode.h"
#include <array>
#include <cstdint>
#include <limits>
#include <istream>
#include <ostream>
#include <vector>

namespace rxpce_state {
inline constexpr std::size_t header_size = 32;
// UUID bytes in printed order, independent of native GUID struct layout.
// HuCard: 69197bd3-65f3-4e45-849e-6fa07e33aa21
inline constexpr std::array<unsigned char, 16> hucard_format = {
    0x69, 0x19, 0x7b, 0xd3, 0x65, 0xf3, 0x4e, 0x45, 0x84, 0x9e, 0x6f, 0xa0, 0x7e, 0x33, 0xaa, 0x21};
// CD: 95949412-dcef-4961-808a-75188907db78
inline constexpr std::array<unsigned char, 16> cd_format = {
    0x95, 0x94, 0x94, 0x12, 0xdc, 0xef, 0x49, 0x61, 0x80, 0x8a, 0x75, 0x18, 0x89, 0x07, 0xdb, 0x78};
// Fast modes have independent identities even though the outer layout is shared.
// fast_hucard_format: 8ee5eb64-8c77-45a6-ac27-402de051c2a4
inline constexpr std::array<unsigned char, 16> fast_hucard_format = {
    0x8e, 0xe5, 0xeb, 0x64, 0x8c, 0x77, 0x45, 0xa6, 0xac, 0x27, 0x40, 0x2d, 0xe0, 0x51, 0xc2, 0xa4};
// fast_cd_format: e6933fbe-fb20-4045-b922-fb272f0dbc84
inline constexpr std::array<unsigned char, 16> fast_cd_format = {
    0xe6, 0x93, 0x3f, 0xbe, 0xfb, 0x20, 0x40, 0x45, 0xb9, 0x22, 0xfb, 0x27, 0x2f, 0x0d, 0xbc, 0x84};
inline std::vector<char> header(std::size_t size, std::uint32_t flags,
                                rxpce_video::VdcMode mode = rxpce_video::VdcMode::accurate) {
    std::vector<char> bytes(header_size, 0);
    const char magic[] = "RXPCEST";
    for (unsigned i = 0; i < 8; ++i)
        bytes[i] = magic[i];
    const bool cd = flags == 0x100 || flags == 0x200;
    const auto& format = mode == rxpce_video::VdcMode::fast
                             ? (cd ? fast_cd_format : fast_hucard_format)
                             : (cd ? cd_format : hucard_format);
    for (unsigned i = 0; i < format.size(); ++i)
        bytes[8 + i] = static_cast<char>(format[i]);
    for (unsigned i = 0; i < 4; ++i) {
        bytes[24 + i] = static_cast<char>(size >> (8 * i));
        bytes[28 + i] = static_cast<char>(flags >> (8 * i));
    }
    return bytes;
}
inline bool write(std::ostream& stream, const std::vector<char>& payload, std::uint32_t flags,
                  rxpce_video::VdcMode mode = rxpce_video::VdcMode::accurate) {
    if (payload.size() > std::numeric_limits<std::uint32_t>::max())
        return false;
    const auto prefix = header(payload.size(), flags, mode);
    stream.write(prefix.data(), prefix.size());
    stream.write(payload.data(), payload.size());
    return static_cast<bool>(stream);
}
inline bool read(std::istream& stream, std::size_t size, std::uint32_t flags,
                 std::vector<char>& payload,
                 rxpce_video::VdcMode mode = rxpce_video::VdcMode::accurate) {
    if (size > std::numeric_limits<std::uint32_t>::max())
        return false;
    const auto expected = header(size, flags, mode);
    std::vector<char> prefix(header_size);
    if (!stream.read(prefix.data(), prefix.size()) || prefix != expected)
        return false;
    std::vector<char> next(size);
    if (!stream.read(next.data(), next.size()))
        return false;
    char extra;
    if (stream.get(extra) || !stream.eof())
        return false;
    payload.swap(next);
    return true;
}
} // namespace rxpce_state
