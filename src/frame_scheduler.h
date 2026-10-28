#pragma once
#include <cstdint>

namespace rxpce_timing {
class FrameScheduler {
  public:
    void reset() {
        *this = FrameScheduler();
    }
    std::int64_t wait_ns(std::int64_t now, bool automatic) const {
        if (!started || !automatic || !previous_auto)
            return 0;
        const auto delay = deadline() - now;
        return delay > 0 ? delay : 0;
    }
    bool begin(std::int64_t now, int minimum_skip, bool automatic) {
        if (minimum_skip < 0)
            minimum_skip = 0;
        if (minimum_skip > 15)
            minimum_skip = 15;
        if (!started || automatic != previous_auto || (automatic && now - deadline() > 250000000)) {
            started = true;
            origin = now;
            frame = 0;
            remaining = consecutive = 0;
        }
        if (minimum_skip != previous_skip || automatic != previous_auto) {
            remaining = consecutive = 0;
            previous_skip = minimum_skip;
            previous_auto = automatic;
        }
        const auto late = now - deadline();
        if (automatic)
            ++frame;
        else {
            origin = now;
            frame = 0;
        }
        if (remaining > 0) {
            --remaining;
            ++consecutive;
            return false;
        }
        if (automatic && late >= 1000000000 / 60 && consecutive < 15) {
            ++consecutive;
            return false;
        }
        consecutive = 0;
        remaining = minimum_skip;
        return true;
    }

  private:
    std::int64_t deadline() const {
        return origin + frame * 1000000000 / 60;
    }
    bool started = false, previous_auto = false;
    std::int64_t origin = 0, frame = 0;
    int remaining = 0, consecutive = 0, previous_skip = -1;
};
} // namespace rxpce_timing
