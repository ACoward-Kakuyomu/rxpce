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
// HuCard: 9698e33d-8052-4041-81fc-ccaa82143a3a
inline constexpr std::array<unsigned char, 16> hucard_format = {
    0x96, 0x98, 0xe3, 0x3d, 0x80, 0x52, 0x40, 0x41, 0x81, 0xfc, 0xcc, 0xaa, 0x82, 0x14, 0x3a, 0x3a};
// CD: a73c94c9-f079-4f69-8397-26bf71ac770b
inline constexpr std::array<unsigned char, 16> cd_format = {
    0xa7, 0x3c, 0x94, 0xc9, 0xf0, 0x79, 0x4f, 0x69, 0x83, 0x97, 0x26, 0xbf, 0x71, 0xac, 0x77, 0x0b};
// Fast modes have independent identities even though the outer layout is shared.
// fast_hucard_format: 78555b63-da2a-4aec-bb4c-1ea45da2b277
inline constexpr std::array<unsigned char, 16> fast_hucard_format = {
    0x78, 0x55, 0x5b, 0x63, 0xda, 0x2a, 0x4a, 0xec, 0xbb, 0x4c, 0x1e, 0xa4, 0x5d, 0xa2, 0xb2, 0x77};
// fast_cd_format: 3b899a45-3bc2-4fab-8115-f30264227289
inline constexpr std::array<unsigned char, 16> fast_cd_format = {
    0x3b, 0x89, 0x9a, 0x45, 0x3b, 0xc2, 0x4f, 0xab, 0x81, 0x15, 0xf3, 0x02, 0x64, 0x22, 0x72, 0x89};
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
