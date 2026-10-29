#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include <istream>
#include <ostream>
#include <vector>

namespace rxpce_state {
inline constexpr std::size_t header_size = 32;
// UUID bytes in printed order, independent of native GUID struct layout.
// HuCard: 0e1b29fa-bc9a-4b0e-a81e-631cf8a6c03b
inline constexpr std::array<unsigned char, 16> hucard_format = {
    0x0e, 0x1b, 0x29, 0xfa, 0xbc, 0x9a, 0x4b, 0x0e, 0xa8, 0x1e, 0x63, 0x1c, 0xf8, 0xa6, 0xc0, 0x3b};
// CD: 6d6806cc-3612-437a-92da-7046432e6cc7
inline constexpr std::array<unsigned char, 16> cd_format = {
    0x6d, 0x68, 0x06, 0xcc, 0x36, 0x12, 0x43, 0x7a, 0x92, 0xda, 0x70, 0x46, 0x43, 0x2e, 0x6c, 0xc7};
inline std::vector<char> header(std::size_t size, std::uint32_t flags) {
    std::vector<char> bytes(header_size, 0);
    const char magic[] = "RXPCEST";
    for (unsigned i = 0; i < 8; ++i)
        bytes[i] = magic[i];
    const auto& format = flags == 0x100 || flags == 0x200 ? cd_format : hucard_format;
    for (unsigned i = 0; i < format.size(); ++i)
        bytes[8 + i] = static_cast<char>(format[i]);
    for (unsigned i = 0; i < 4; ++i) {
        bytes[24 + i] = static_cast<char>(size >> (8 * i));
        bytes[28 + i] = static_cast<char>(flags >> (8 * i));
    }
    return bytes;
}
inline bool write(std::ostream& stream, const std::vector<char>& payload, std::uint32_t flags) {
    if (payload.size() > std::numeric_limits<std::uint32_t>::max())
        return false;
    const auto prefix = header(payload.size(), flags);
    stream.write(prefix.data(), prefix.size());
    stream.write(payload.data(), payload.size());
    return static_cast<bool>(stream);
}
inline bool read(std::istream& stream, std::size_t size, std::uint32_t flags,
                 std::vector<char>& payload) {
    if (size > std::numeric_limits<std::uint32_t>::max())
        return false;
    const auto expected = header(size, flags);
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
