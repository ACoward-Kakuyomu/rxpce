#pragma once
#include "unique_handle.h"
#include <atomic>
#include <cstdint>

namespace rxpce_host {
class CommandChannel {
  public:
    // The UI initializes before launching the consumer, and resets after join.
    // Only post/take may run concurrently; the event's lifetime is immutable.
    bool initialize() noexcept {
        reset();
        event_.reset(CreateEvent(nullptr, FALSE, FALSE, nullptr));
        return ready();
    }
    void reset() noexcept {
        event_.reset();
        pending_ = 0;
    }
    bool ready() const noexcept {
        return bool(event_);
    }
    void post(std::uint32_t commands) noexcept {
        if (!ready() || !commands)
            return;
        pending_.fetch_or(commands);
        SetEvent(event_.get());
    }
    std::uint32_t take() noexcept {
        if (!ready() || WaitForSingleObject(event_.get(), 0) != WAIT_OBJECT_0)
            return 0;
        return pending_.exchange(0);
    }

  private:
    UniqueHandle event_;
    std::atomic<std::uint32_t> pending_{0};
};
} // namespace rxpce_host
