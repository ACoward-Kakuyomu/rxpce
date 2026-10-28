#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <limits>
namespace rxpce_state {
struct Writer {
    std::vector<char> data;
    void number(std::uint64_t value, unsigned bytes) {
        if (bytes < 1 || bytes > 8)
            throw std::invalid_argument("Invalid state number width");
        for (unsigned n = 0; n < bytes; ++n)
            data.push_back(static_cast<char>(value >> (n * 8)));
    }
    void block(const void* ptr, std::size_t size) {
        if (!size)
            return;
        if (!ptr)
            throw std::invalid_argument("Null state block");
        auto p = static_cast<const char*>(ptr);
        data.insert(data.end(), p, p + size);
    }
    void real(double value) {
        std::uint64_t bits;
        static_assert(sizeof(value) == sizeof(bits) && std::numeric_limits<double>::is_iec559,
                      "IEEE 64-bit double required");
        std::memcpy(&bits, &value, 8);
        number(bits, 8);
    }
};
struct Reader {
    const char* cursor;
    std::size_t remaining;
    Reader(const char* data, std::size_t size) : cursor(data), remaining(size) {
        if (!data && size)
            throw std::invalid_argument("Null state input");
    }
    void block(void* ptr, std::size_t size) {
        if (!size)
            return;
        if (!ptr)
            throw std::invalid_argument("Null state destination");
        if (size > remaining)
            throw std::runtime_error("Truncated state");
        std::memcpy(ptr, cursor, size);
        cursor += size;
        remaining -= size;
    }
    std::uint64_t number(unsigned bytes) {
        if (bytes < 1 || bytes > 8 || bytes > remaining)
            throw std::runtime_error("Invalid or truncated state number");
        std::uint64_t v = 0;
        for (unsigned n = 0; n < bytes; ++n)
            v |= std::uint64_t(static_cast<unsigned char>(*cursor++)) << (8 * n);
        remaining -= bytes;
        return v;
    }
    std::int64_t signed_number(unsigned bytes) {
        const auto value = number(bytes), maximum = bytes == 8
                                                        ? std::uint64_t(INT64_MAX)
                                                        : (std::uint64_t(1) << (bytes * 8 - 1)) - 1;
        return value <= maximum
                   ? static_cast<std::int64_t>(value)
                   : -1 - static_cast<std::int64_t>(
                              (bytes == 8 ? UINT64_MAX : (std::uint64_t(1) << (bytes * 8)) - 1) -
                              value);
    }
    bool boolean() {
        const auto v = number(1);
        if (v > 1)
            throw std::runtime_error("Invalid boolean state");
        return v != 0;
    }
    double real() {
        const auto bits = number(8);
        double value;
        std::memcpy(&value, &bits, 8);
        return value;
    }
};
} // namespace rxpce_state
