#pragma once
#include "win32_support.h"
#include <avrt.h>
#include <cstdint>

namespace rxpce_host {
// Use and destroy on the registering thread. The task name must outlive this object.
class MultimediaTask {
  public:
    using Register = HANDLE(WINAPI*)(LPCWSTR, LPDWORD);
    using Revert = BOOL(WINAPI*)(HANDLE);
    explicit MultimediaTask(const wchar_t* name, Register start = AvSetMmThreadCharacteristicsW,
                            Revert stop = AvRevertMmThreadCharacteristics)
        : name_(name), start_(start), stop_(stop) {}
    MultimediaTask(const MultimediaTask&) = delete;
    MultimediaTask& operator=(const MultimediaTask&) = delete;
    ~MultimediaTask() {
        release();
    }
    bool set_enabled(bool enabled) {
        if (initialized_ && enabled_ == enabled)
            return false;
        initialized_ = true;
        enabled_ = enabled;
        release();
        if (enabled) {
            DWORD index = 0;
            task_ = start_(name_, &index);
        }
        return true;
    }
    bool active() const {
        return task_ != nullptr;
    }

  private:
    void release() {
        if (task_) {
            stop_(task_);
            task_ = nullptr;
        }
    }
    const wchar_t* name_;
    Register start_;
    Revert stop_;
    HANDLE task_ = nullptr;
    bool initialized_ = false, enabled_ = false;
};
} // namespace rxpce_host
