#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rxpce_audio {
// Original bounded, causal output converter. Times are output-frame coordinates.
class BandlimitedBuffer {
    static constexpr unsigned phases = 256;
    struct Kernel {
        unsigned rate, pcm_rate;
        double delay;
        std::size_t taps, capacity = 1;
        std::vector<double> coefficients;
        Kernel(unsigned output, unsigned input) : rate(output), pcm_rate(input) {
            constexpr double pi = 3.14159265358979323846;
            const double minimum = (std::min)(output, 44100u);
            delay = 32 * output / minimum;
            // PCM is supplied at the end of its native interval; the controller
            // accounts for that one-sample availability delay separately.
            if (input)
                delay -= double(output) / input;
            const double width = delay * 2;
            const double cutoff = 0.45 * (input ? minimum / output : 1);
            taps = static_cast<std::size_t>(std::ceil(width)) + 1;
            while (capacity < taps + 2)
                capacity *= 2;
            const auto impulse = [&](double time) {
                const double x = time - delay;
                if (std::fabs(x) >= delay)
                    return 0.0;
                const double a = 2 * pi * cutoff * x;
                const double sinc = a == 0 ? 1 : std::sin(a) / a;
                const double window =
                    0.42 + 0.5 * std::cos(pi * x / delay) + 0.08 * std::cos(2 * pi * x / delay);
                return 2 * cutoff * sinc * window;
            };
            // Integrate the continuous impulse on a fine grid. This is coefficient design,
            // performed at configuration time, never in the playback callback.
            const auto points = static_cast<std::size_t>(std::ceil(width * phases));
            std::vector<double> area(points + 1);
            double previous = impulse(0);
            for (std::size_t n = 1; n <= points; ++n) {
                const double value = impulse(double(n) / phases);
                area[n] = area[n - 1] + (previous + value) / (2 * phases);
                previous = value;
            }
            const double dc = area.back();
            const auto step = [&](double time) {
                if (time <= 0)
                    return 0.0;
                if (time >= width)
                    return 1.0;
                const double position = time * phases;
                const auto index = static_cast<std::size_t>(position);
                return (area[index] + (area[index + 1] - area[index]) * (position - index)) / dc;
            };
            coefficients.resize((phases + 1) * taps);
            for (unsigned phase = 0; phase <= phases; ++phase) {
                const double fraction = double(phase) / phases;
                for (std::size_t n = 0; n < taps; ++n) {
                    const double time = n + 1 - fraction;
                    const double value =
                        input ? impulse(time) / dc * output / input : step(time) - step(time - 1);
                    coefficients[phase * taps + n] = value;
                }
            }
        }
    };
    static std::shared_ptr<const Kernel> kernel(unsigned rate, unsigned pcm_rate) {
        if (rate < 1000 || rate > 192000 || (pcm_rate && pcm_rate != 44100))
            throw std::invalid_argument("Invalid bandlimited audio rate");
        static std::mutex mutex;
        // A bounded cache also limits memory when untrusted states contain many rates.
        static std::array<std::shared_ptr<const Kernel>, 16> cache;
        static unsigned replacement = 0;
        const std::lock_guard<std::mutex> lock(mutex);
        for (const auto& item : cache)
            if (item && item->rate == rate && item->pcm_rate == pcm_rate)
                return item;
        auto& item = cache[replacement];
        auto next = std::make_shared<const Kernel>(rate, pcm_rate);
        item = std::move(next);
        replacement = (replacement + 1) % cache.size();
        return item;
    }

  public:
    struct State {
        unsigned rate = 0, pcm_rate = 0;
        std::size_t cursor = 0;
        std::array<double, 2> integral{};
        std::vector<std::array<double, 2>> tail;
    };
    void configure(unsigned rate, unsigned pcm_rate = 0) {
        auto next = kernel(rate, pcm_rate);
        State state;
        state.rate = rate;
        state.pcm_rate = pcm_rate;
        state.tail.resize(next->capacity);
        kernel_ = std::move(next);
        state_ = std::move(state);
    }
    double delay_frames() const {
        return kernel_ ? kernel_->delay : 0;
    }
    std::size_t taps() const {
        return kernel_ ? kernel_->taps : 0;
    }
    void clear() {
        std::fill(state_.tail.begin(), state_.tail.end(), std::array<double, 2>{});
        state_.cursor = 0;
        state_.integral = {};
    }
    void step(unsigned offset, double phase, double left, double right) {
        if (!kernel_ || kernel_->pcm_rate)
            throw std::logic_error("Step input requires a step converter");
        add(offset, phase, left, right);
    }
    void impulse(unsigned offset, double phase, double left, double right) {
        if (!kernel_ || !kernel_->pcm_rate)
            throw std::logic_error("PCM input requires a PCM converter");
        add(offset, phase, left, right);
    }
    std::array<float, 2> read() {
        if (!kernel_)
            throw std::logic_error("Audio converter is not configured");
        auto& pair = state_.tail[state_.cursor];
        std::array<float, 2> output;
        for (unsigned ch = 0; ch < 2; ++ch) {
            if (!kernel_->pcm_rate)
                state_.integral[ch] += pair[ch];
            output[ch] = static_cast<float>(kernel_->pcm_rate ? pair[ch] : state_.integral[ch]);
        }
        pair = {};
        state_.cursor = (state_.cursor + 1) & (state_.tail.size() - 1);
        return output;
    }
    State state() const {
        return state_;
    }
    bool restore(const State& state) {
        if (state.rate < 1000 || state.rate > 192000 || (state.pcm_rate && state.pcm_rate != 44100))
            return false;
        auto next = kernel(state.rate, state.pcm_rate);
        if (state.tail.size() != next->capacity || state.cursor >= state.tail.size())
            return false;
        const auto valid = [](double value) {
            return std::isfinite(value) && std::fabs(value) <= 1e20;
        };
        for (double value : state.integral)
            if (!valid(value) || (state.pcm_rate && value != 0))
                return false;
        for (const auto& pair : state.tail)
            for (double value : pair)
                if (!valid(value))
                    return false;
        State copy = state;
        kernel_ = std::move(next);
        state_ = std::move(copy);
        return true;
    }

  private:
    std::shared_ptr<const Kernel> kernel_;
    State state_;
    void add(unsigned offset, double phase, double left, double right) {
        if (!std::isfinite(phase) || phase < 0 || phase >= 1 ||
            offset > state_.tail.size() - kernel_->taps || !std::isfinite(left) ||
            !std::isfinite(right) || std::fabs(left) > 1e20 || std::fabs(right) > 1e20)
            throw std::invalid_argument("Invalid bandlimited audio event");
        if (left == 0 && right == 0)
            return;
        const double position = phase * phases;
        const auto index = static_cast<unsigned>(position);
        const double fraction = position - index;
        const auto* first = kernel_->coefficients.data() + index * kernel_->taps;
        const auto* second = first + kernel_->taps;
        const auto mask = state_.tail.size() - 1;
        for (std::size_t n = 0; n < kernel_->taps; ++n) {
            const double coefficient = first[n] + (second[n] - first[n]) * fraction;
            auto& pair = state_.tail[(state_.cursor + offset + n) & mask];
            pair[0] += left * coefficient;
            pair[1] += right * coefficient;
        }
    }
};
} // namespace rxpce_audio
