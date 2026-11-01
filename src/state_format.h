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
// HuCard: 7ffb12a6-75c3-4156-b3fb-631cc5618300
inline constexpr std::array<unsigned char, 16> hucard_format = {
    0x7f, 0xfb, 0x12, 0xa6, 0x75, 0xc3, 0x41, 0x56, 0xb3, 0xfb, 0x63, 0x1c, 0xc5, 0x61, 0x83, 0x00};
// CD: cef4c1d2-ef6d-474a-9ef6-e6a659084867
inline constexpr std::array<unsigned char, 16> cd_format = {
    0xce, 0xf4, 0xc1, 0xd2, 0xef, 0x6d, 0x47, 0x4a, 0x9e, 0xf6, 0xe6, 0xa6, 0x59, 0x08, 0x48, 0x67};
// Fast modes have independent identities even though the outer layout is shared.
// fast_hucard_format: c8f96ba0-11f0-47a5-b855-60d91c9347d1
inline constexpr std::array<unsigned char, 16> fast_hucard_format = {
    0xc8, 0xf9, 0x6b, 0xa0, 0x11, 0xf0, 0x47, 0xa5, 0xb8, 0x55, 0x60, 0xd9, 0x1c, 0x93, 0x47, 0xd1};
// fast_cd_format: 741429b5-d153-4fa4-9c34-e3d5cbc4fa33
inline constexpr std::array<unsigned char, 16> fast_cd_format = {
    0x74, 0x14, 0x29, 0xb5, 0xd1, 0x53, 0x4f, 0xa4, 0x9c, 0x34, 0xe3, 0xd5, 0xcb, 0xc4, 0xfa, 0x33};
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
