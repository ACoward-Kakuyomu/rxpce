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
// HuCard: 0a78d029-1bd5-4021-ae34-dfe362e27511
inline constexpr std::array<unsigned char, 16> hucard_format = {
    0x0a, 0x78, 0xd0, 0x29, 0x1b, 0xd5, 0x40, 0x21, 0xae, 0x34, 0xdf, 0xe3, 0x62, 0xe2, 0x75, 0x11};
// CD: 11371f7f-6bdc-430f-9b7a-38774e677f3c
inline constexpr std::array<unsigned char, 16> cd_format = {
    0x11, 0x37, 0x1f, 0x7f, 0x6b, 0xdc, 0x43, 0x0f, 0x9b, 0x7a, 0x38, 0x77, 0x4e, 0x67, 0x7f, 0x3c};
// Fast modes have independent identities even though the outer layout is shared.
// fast_hucard_format: dc3397e7-9fa1-4638-8466-205f24c42e1d
inline constexpr std::array<unsigned char, 16> fast_hucard_format = {
    0xdc, 0x33, 0x97, 0xe7, 0x9f, 0xa1, 0x46, 0x38, 0x84, 0x66, 0x20, 0x5f, 0x24, 0xc4, 0x2e, 0x1d};
// fast_cd_format: bf1115fd-0c87-4907-8113-f13267a2f36a
inline constexpr std::array<unsigned char, 16> fast_cd_format = {
    0xbf, 0x11, 0x15, 0xfd, 0x0c, 0x87, 0x49, 0x07, 0x81, 0x13, 0xf1, 0x32, 0x67, 0xa2, 0xf3, 0x6a};
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
