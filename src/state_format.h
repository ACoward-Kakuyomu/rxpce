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
// HuCard: 3f3b9670-c5e6-4374-aa9a-55ce80a2d37e
inline constexpr std::array<unsigned char, 16> hucard_format = {
    0x3f, 0x3b, 0x96, 0x70, 0xc5, 0xe6, 0x43, 0x74, 0xaa, 0x9a, 0x55, 0xce, 0x80, 0xa2, 0xd3, 0x7e};
// CD: 57072918-8cd5-40de-9062-6ae8db5f587a
inline constexpr std::array<unsigned char, 16> cd_format = {
    0x57, 0x07, 0x29, 0x18, 0x8c, 0xd5, 0x40, 0xde, 0x90, 0x62, 0x6a, 0xe8, 0xdb, 0x5f, 0x58, 0x7a};
// Fast modes have independent identities even though the outer layout is shared.
// fast_hucard_format: c145d662-78bc-4f18-b558-046b472d2aa3
inline constexpr std::array<unsigned char, 16> fast_hucard_format = {
    0xc1, 0x45, 0xd6, 0x62, 0x78, 0xbc, 0x4f, 0x18, 0xb5, 0x58, 0x04, 0x6b, 0x47, 0x2d, 0x2a, 0xa3};
// fast_cd_format: 88ab1a46-889d-4e5d-b776-4521e5d53899
inline constexpr std::array<unsigned char, 16> fast_cd_format = {
    0x88, 0xab, 0x1a, 0x46, 0x88, 0x9d, 0x4e, 0x5d, 0xb7, 0x76, 0x45, 0x21, 0xe5, 0xd5, 0x38, 0x99};
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
