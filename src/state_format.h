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
// HuCard: 9d7b716c-4081-4b03-a8e3-5aea117cdb98
inline constexpr std::array<unsigned char, 16> hucard_format = {
    0x9d, 0x7b, 0x71, 0x6c, 0x40, 0x81, 0x4b, 0x03, 0xa8, 0xe3, 0x5a, 0xea, 0x11, 0x7c, 0xdb, 0x98};
// CD: 490c0855-1560-4417-9f6f-aa6678ed1f4c
inline constexpr std::array<unsigned char, 16> cd_format = {
    0x49, 0x0c, 0x08, 0x55, 0x15, 0x60, 0x44, 0x17, 0x9f, 0x6f, 0xaa, 0x66, 0x78, 0xed, 0x1f, 0x4c};
// Fast modes have independent identities even though the outer layout is shared.
// fast_hucard_format: 5fb92b5a-c696-4864-bf69-234444a719b7
inline constexpr std::array<unsigned char, 16> fast_hucard_format = {
    0x5f, 0xb9, 0x2b, 0x5a, 0xc6, 0x96, 0x48, 0x64, 0xbf, 0x69, 0x23, 0x44, 0x44, 0xa7, 0x19, 0xb7};
// fast_cd_format: 0e544f87-6385-4774-aa4d-c47169893037
inline constexpr std::array<unsigned char, 16> fast_cd_format = {
    0x0e, 0x54, 0x4f, 0x87, 0x63, 0x85, 0x47, 0x74, 0xaa, 0x4d, 0xc4, 0x71, 0x69, 0x89, 0x30, 0x37};
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
