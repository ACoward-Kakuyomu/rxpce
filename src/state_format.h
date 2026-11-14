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
// HuCard: 249a50ca-c202-4252-a80b-b86c6a6481c0
inline constexpr std::array<unsigned char, 16> hucard_format = {
    0x24, 0x9a, 0x50, 0xca, 0xc2, 0x02, 0x42, 0x52, 0xa8, 0x0b, 0xb8, 0x6c, 0x6a, 0x64, 0x81, 0xc0};
// CD: 6cdc8647-1deb-44d5-b550-9eb6163dd999
inline constexpr std::array<unsigned char, 16> cd_format = {
    0x6c, 0xdc, 0x86, 0x47, 0x1d, 0xeb, 0x44, 0xd5, 0xb5, 0x50, 0x9e, 0xb6, 0x16, 0x3d, 0xd9, 0x99};
// Fast modes have independent identities even though the outer layout is shared.
// fast_hucard_format: dedf7184-1e67-4720-98b6-793fb0aa8c20
inline constexpr std::array<unsigned char, 16> fast_hucard_format = {
    0xde, 0xdf, 0x71, 0x84, 0x1e, 0x67, 0x47, 0x20, 0x98, 0xb6, 0x79, 0x3f, 0xb0, 0xaa, 0x8c, 0x20};
// fast_cd_format: 6af8c617-b86b-48f5-8e6e-730b53c950a6
inline constexpr std::array<unsigned char, 16> fast_cd_format = {
    0x6a, 0xf8, 0xc6, 0x17, 0xb8, 0x6b, 0x48, 0xf5, 0x8e, 0x6e, 0x73, 0x0b, 0x53, 0xc9, 0x50, 0xa6};
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
