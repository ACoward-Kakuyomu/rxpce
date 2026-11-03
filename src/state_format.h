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
// HuCard: 3eef95c0-b15a-45d6-a149-3be645557745
inline constexpr std::array<unsigned char, 16> hucard_format = {
    0x3e, 0xef, 0x95, 0xc0, 0xb1, 0x5a, 0x45, 0xd6, 0xa1, 0x49, 0x3b, 0xe6, 0x45, 0x55, 0x77, 0x45};
// CD: fa8e97d9-05e5-4b4a-91ce-6ee90b3eddea
inline constexpr std::array<unsigned char, 16> cd_format = {
    0xfa, 0x8e, 0x97, 0xd9, 0x05, 0xe5, 0x4b, 0x4a, 0x91, 0xce, 0x6e, 0xe9, 0x0b, 0x3e, 0xdd, 0xea};
// Fast modes have independent identities even though the outer layout is shared.
// fast_hucard_format: 619303ee-dbb4-4b5d-9118-0f63ee1858ec
inline constexpr std::array<unsigned char, 16> fast_hucard_format = {
    0x61, 0x93, 0x03, 0xee, 0xdb, 0xb4, 0x4b, 0x5d, 0x91, 0x18, 0x0f, 0x63, 0xee, 0x18, 0x58, 0xec};
// fast_cd_format: 4a48bc84-9e4f-43cd-a5f1-643547e6f99f
inline constexpr std::array<unsigned char, 16> fast_cd_format = {
    0x4a, 0x48, 0xbc, 0x84, 0x9e, 0x4f, 0x43, 0xcd, 0xa5, 0xf1, 0x64, 0x35, 0x47, 0xe6, 0xf9, 0x9f};
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
