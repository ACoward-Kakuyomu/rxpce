#pragma once
#include <cstdint>
namespace rxpce_audio {
// Host diagnostics only; timestamps are monotonic milliseconds, not emulation time.
class ErrorLog {
  public:
    enum class Kind { failure, repeated, recovered };
    struct Event {
        Kind kind;
        std::uint32_t code;
        std::uint64_t count;
    };
    bool active() const noexcept {
        return active_;
    }
    template <class Emit> void error(std::uint32_t code, std::uint64_t now, Emit emit) {
        healthy_ = false;
        if (!active_ || code != code_) {
            finish(emit);
            active_ = true;
            code_ = code;
            reported_at_ = now;
            emit(Event{Kind::failure, code_, 0});
        } else {
            ++repeated_;
            if (now - reported_at_ >= 30000) {
                finish(emit);
                reported_at_ = now;
            }
        }
    }
    template <class Emit> void success(std::uint64_t now, Emit emit) {
        if (!active_)
            return;
        if (!healthy_) {
            healthy_ = true;
            healthy_at_ = now;
        }
        if (now - healthy_at_ >= 2000) {
            finish(emit);
            emit(Event{Kind::recovered, code_, 0});
            active_ = false;
            healthy_ = false;
        }
    }
    template <class Emit> void finish(Emit emit) {
        if (repeated_) {
            emit(Event{Kind::repeated, code_, repeated_});
            repeated_ = 0;
        }
    }

  private:
    bool active_ = false, healthy_ = false;
    std::uint32_t code_ = 0;
    std::uint64_t repeated_ = 0, reported_at_ = 0, healthy_at_ = 0;
};
} // namespace rxpce_audio
