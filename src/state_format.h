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
// HuCard: ddbc57ba-448d-4ccc-ab1f-0146aee328c4
inline constexpr std::array<unsigned char, 16> hucard_format = {
    0xdd, 0xbc, 0x57, 0xba, 0x44, 0x8d, 0x4c, 0xcc, 0xab, 0x1f, 0x01, 0x46, 0xae, 0xe3, 0x28, 0xc4};
// CD: d540a6df-1493-4203-9899-c5e6bc204f11
inline constexpr std::array<unsigned char, 16> cd_format = {
    0xd5, 0x40, 0xa6, 0xdf, 0x14, 0x93, 0x42, 0x03, 0x98, 0x99, 0xc5, 0xe6, 0xbc, 0x20, 0x4f, 0x11};
// Fast modes have independent identities even though the outer layout is shared.
// fast_hucard_format: d2c84836-3523-4349-8621-2fbba5d5c40a
inline constexpr std::array<unsigned char, 16> fast_hucard_format = {
    0xd2, 0xc8, 0x48, 0x36, 0x35, 0x23, 0x43, 0x49, 0x86, 0x21, 0x2f, 0xbb, 0xa5, 0xd5, 0xc4, 0x0a};
// fast_cd_format: 452848d7-10bc-480f-8a9e-eb447f78edea
inline constexpr std::array<unsigned char, 16> fast_cd_format = {
    0x45, 0x28, 0x48, 0xd7, 0x10, 0xbc, 0x48, 0x0f, 0x8a, 0x9e, 0xeb, 0x44, 0x7f, 0x78, 0xed, 0xea};
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
