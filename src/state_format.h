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
// HuCard: 7240fa4d-bcbe-4910-b60c-b733b0346bb1
inline constexpr std::array<unsigned char, 16> hucard_format = {
    0x72, 0x40, 0xfa, 0x4d, 0xbc, 0xbe, 0x49, 0x10, 0xb6, 0x0c, 0xb7, 0x33, 0xb0, 0x34, 0x6b, 0xb1};
// CD: 63f47d22-2904-4ab3-9dea-53abddc76048
inline constexpr std::array<unsigned char, 16> cd_format = {
    0x63, 0xf4, 0x7d, 0x22, 0x29, 0x04, 0x4a, 0xb3, 0x9d, 0xea, 0x53, 0xab, 0xdd, 0xc7, 0x60, 0x48};
// Fast modes have independent identities even though the outer layout is shared.
// fast_hucard_format: ca9215fc-9c0c-4fe2-9b63-184bc7158c25
inline constexpr std::array<unsigned char, 16> fast_hucard_format = {
    0xca, 0x92, 0x15, 0xfc, 0x9c, 0x0c, 0x4f, 0xe2, 0x9b, 0x63, 0x18, 0x4b, 0xc7, 0x15, 0x8c, 0x25};
// fast_cd_format: d4bb2b24-ac03-40b5-9b61-f44f17d52654
inline constexpr std::array<unsigned char, 16> fast_cd_format = {
    0xd4, 0xbb, 0x2b, 0x24, 0xac, 0x03, 0x40, 0xb5, 0x9b, 0x61, 0xf4, 0x4f, 0x17, 0xd5, 0x26, 0x54};
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
