#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace rxpce_debug {
class CommandLine {
  public:
    static constexpr std::size_t capacity = 1023;
    void clear() noexcept {
        size_ = 0;
    }
    bool append(char ch) noexcept {
        if (ch < ' ' || ch > '~' || size_ == capacity)
            return false;
        bytes_[size_++] = ch;
        return true;
    }
    void pop() noexcept {
        if (size_)
            --size_;
    }
    std::string_view text() const noexcept {
        return {bytes_.data(), size_};
    }
    char command() const noexcept {
        return size_ ? bytes_[0] : 0;
    }
    std::optional<std::uint16_t> address() const noexcept {
        std::uint16_t value = 0;
        std::size_t at = 1;
        for (; at < size_; ++at) {
            const auto ch = bytes_[at];
            unsigned digit;
            if (ch >= '0' && ch <= '9')
                digit = ch - '0';
            else if (ch >= 'A' && ch <= 'F')
                digit = ch - 'A' + 10;
            else if (ch >= 'a' && ch <= 'f')
                digit = ch - 'a' + 10;
            else
                break;
            value = static_cast<std::uint16_t>(value * 16u + digit);
        }
        return at > 1 ? std::optional<std::uint16_t>(value) : std::nullopt;
    }

  private:
    std::array<char, capacity> bytes_{};
    std::size_t size_ = 0;
};
} // namespace rxpce_debug
