#pragma once
#include <cstdint>
#include <istream>
#include <ostream>
#include <vector>

namespace rxpce_state {
inline std::vector<char> header(std::size_t size, std::uint32_t flags) {
    std::vector<char> bytes = {'R', 'X', 'P', 'C', 'E', '0', '0', '8', 0, 0, 0, 0, 0, 0, 0, 0};
    if (flags == 0x100 || flags == 0x200)
        bytes[7] = '9';
    for (unsigned i = 0; i < 4; ++i)
        bytes[8 + i] = static_cast<char>(size >> (8 * i));
    for (unsigned i = 0; i < 4; ++i)
        bytes[12 + i] = static_cast<char>(flags >> (8 * i));
    return bytes;
}
inline bool write(std::ostream& stream, const std::vector<char>& payload, std::uint32_t flags) {
    const auto prefix = header(payload.size(), flags);
    stream.write(prefix.data(), prefix.size());
    stream.write(payload.data(), payload.size());
    return static_cast<bool>(stream);
}
inline bool read(std::istream& stream, std::size_t size, std::uint32_t flags,
                 std::vector<char>& payload) {
    const auto expected = header(size, flags);
    std::vector<char> prefix(16), next(size);
    if (!stream.read(prefix.data(), prefix.size()) || prefix != expected ||
        !stream.read(next.data(), next.size()))
        return false;
    char extra;
    if (stream.get(extra) || !stream.eof())
        return false;
    payload.swap(next);
    return true;
}
} // namespace rxpce_state
