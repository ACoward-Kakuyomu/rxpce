#pragma once
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace rxpce_timing {
class FrameScheduler {
  public:
    void reset() {
        started = previous_auto = false;
        origin = 0;
        origin_ticks = last_ticks = next_ticks = 0;
        remaining = consecutive = 0;
        previous_skip = -1;
    }
    void set_frame_clock(unsigned cycles, unsigned clock_hz) {
        if (!cycles || !clock_hz)
            throw std::invalid_argument("Invalid frame clock");
        period = cycles;
        denominator = clock_hz;
        reset();
    }
    // Fixed-period clients may omit timestamps. Video clients can supply completed ticks.
    std::int64_t wait_ns(std::int64_t now, bool automatic) const {
        return wait_ns(now, automatic, next_ticks);
    }
    bool begin(std::int64_t now, int skip, bool automatic) {
        const auto ticks = next_ticks;
        if (ticks > (std::numeric_limits<std::uint64_t>::max)() - period)
            throw std::overflow_error("Frame tick overflow");
        const auto draw = begin(now, skip, automatic, ticks);
        next_ticks = ticks + period;
        return draw;
    }
    std::int64_t wait_ns(std::int64_t now, bool automatic, std::uint64_t ticks) const {
        if (!started || !automatic || !previous_auto)
            return 0;
        const auto end = deadline(ticks);
        return end > now ? end - now : 0;
    }
    bool begin(std::int64_t now, int minimum_skip, bool automatic, std::uint64_t ticks) {
        if (started && ticks < last_ticks)
            throw std::invalid_argument("Frame time moved backwards without reset");
        if (minimum_skip < 0)
            minimum_skip = 0;
        if (minimum_skip > 15)
            minimum_skip = 15;
        const auto duration = started && ticks > last_ticks ? ticks - last_ticks : period;
        const auto frame_ns = nanoseconds(duration);
        if (!started || automatic != previous_auto ||
            (automatic && now > deadline(ticks) && now - deadline(ticks) > 250000000)) {
            started = true;
            origin = now;
            origin_ticks = ticks;
            remaining = consecutive = 0;
        }
        if (minimum_skip != previous_skip || automatic != previous_auto) {
            remaining = consecutive = 0;
            previous_skip = minimum_skip;
            previous_auto = automatic;
        }
        const auto late = now - deadline(ticks);
        last_ticks = ticks;
        if (remaining > 0) {
            --remaining;
            ++consecutive;
            return false;
        }
        if (automatic && late >= frame_ns && consecutive < 15) {
            ++consecutive;
            return false;
        }
        consecutive = 0;
        remaining = minimum_skip;
        return true;
    }

  private:
    std::int64_t nanoseconds(std::uint64_t ticks) const {
        constexpr auto maximum = std::uint64_t((std::numeric_limits<std::int64_t>::max)());
        const auto whole = ticks / denominator;
        const auto small = (ticks % denominator) * 1000000000 / denominator;
        if (whole > maximum / 1000000000 || whole * 1000000000 > maximum - small)
            throw std::overflow_error("Frame interval overflow");
        return static_cast<std::int64_t>(whole * 1000000000 + small);
    }
    std::int64_t deadline(std::uint64_t ticks) const {
        if (ticks < origin_ticks || ticks < last_ticks)
            throw std::invalid_argument("Frame time moved backwards without reset");
        const auto elapsed = nanoseconds(ticks - origin_ticks);
        if (origin > (std::numeric_limits<std::int64_t>::max)() - elapsed)
            throw std::overflow_error("Frame deadline overflow");
        return origin + elapsed;
    }
    unsigned period = 1, denominator = 60;
    bool started = false, previous_auto = false;
    std::int64_t origin = 0;
    std::uint64_t origin_ticks = 0, last_ticks = 0, next_ticks = 0;
    int remaining = 0, consecutive = 0, previous_skip = -1;
};
} // namespace rxpce_timing
