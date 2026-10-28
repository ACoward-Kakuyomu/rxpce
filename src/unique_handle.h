#pragma once
#include <windows.h>
#include <utility>

namespace rxpce_host {
// Owns only CloseHandle resources. HWND, COM, MMCSS tasks and borrowed handles
// have different lifetimes and must not be passed to this type.
class UniqueHandle {
  public:
    UniqueHandle() noexcept = default;
    explicit UniqueHandle(HANDLE value) noexcept : value_(value) {}
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    UniqueHandle(UniqueHandle&& other) noexcept : value_(other.release()) {}
    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other)
            reset(other.release());
        return *this;
    }
    ~UniqueHandle() {
        reset();
    }
    HANDLE get() const noexcept {
        return value_;
    }
    explicit operator bool() const noexcept {
        return value_ && value_ != INVALID_HANDLE_VALUE;
    }
    HANDLE release() noexcept {
        return std::exchange(value_, nullptr);
    }
    void reset(HANDLE next = nullptr) noexcept {
        if (next == value_)
            return;
        if (*this) {
            const auto error = GetLastError();
            CloseHandle(value_);
            SetLastError(error);
        }
        value_ = next;
    }

  private:
    HANDLE value_ = nullptr;
};
} // namespace rxpce_host
