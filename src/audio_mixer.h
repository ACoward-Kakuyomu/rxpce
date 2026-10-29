#pragma once
#include "psg_state.h"
#include "psg_synth.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include <functional>
#include <utility>

namespace rxpce_audio {

inline std::int16_t pcm16(float value) {
    if (!std::isfinite(value))
        return 0;
    const float scaled = value * 32768.0f;
    if (scaled >= 32767.0f)
        return 32767;
    if (scaled <= -32768.0f)
        return -32768;
    return static_cast<std::int16_t>(scaled);
}

inline void write_pcm16(const float* source, std::int16_t* first, std::size_t first_count,
                        std::int16_t* second, std::size_t second_count) {
    for (std::size_t i = 0; i < first_count; ++i)
        first[i] = pcm16(source[i]);
    for (std::size_t i = 0; i < second_count; ++i)
        second[i] = pcm16(source[first_count + i]);
}

class SampleQueue {
    std::vector<float> samples_;
    std::size_t read_ = 0, size_ = 0;

  public:
    void reset(std::size_t capacity) {
        samples_.assign(capacity, 0);
        clear();
    }
    void clear() {
        read_ = size_ = 0;
    }
    std::size_t queued() const {
        return size_;
    }

    // Keep the most recent samples when the CPU runs ahead of playback.
    template <class Generate> void append(std::size_t count, Generate generate) {
        if (samples_.empty())
            return;
        while (count) {
            const auto chunk = (std::min)(count, samples_.size());
            const auto discard =
                chunk > samples_.size() - size_ ? chunk - (samples_.size() - size_) : 0;
            read_ = (read_ + discard) % samples_.size();
            size_ -= discard;
            auto remaining = chunk;
            while (remaining) {
                const auto write = (read_ + size_) % samples_.size();
                const auto n = (std::min)(remaining, samples_.size() - write);
                generate(samples_.data() + write, n);
                size_ += n;
                remaining -= n;
            }
            count -= chunk;
        }
    }

    void read(float* out, std::size_t count) {
        while (count && size_) {
            const auto n = (std::min)(count, (std::min)(size_, samples_.size() - read_));
            std::copy_n(samples_.data() + read_, n, out);
            out += n;
            count -= n;
            size_ -= n;
            read_ = (read_ + n) % samples_.size();
        }
        if (count)
            std::fill_n(out, count, 0.0f);
    }
};

class AudioClock {
    std::uint32_t last_ = 0;
    std::uint64_t cycles_ = 0;

  public:
    void reset(std::uint32_t cycles) {
        last_ = cycles;
        cycles_ = 0;
    }
    bool update(std::uint32_t cycles) {
        const std::uint32_t elapsed = cycles - last_;
        if (elapsed > 0x7fffffffu) {
            reset(cycles);
            return true;
        }
        cycles_ += elapsed;
        last_ = cycles;
        return false;
    }
    std::uint64_t elapsed() const {
        return cycles_;
    }
};

class Mixer {
    std::function<void(float*, std::size_t, unsigned)> extra_source_;
    PsgState state_;
    PsgSynth synth_;
    SampleQueue ready_;
    std::vector<float> samples_, extra_samples_;
    AudioClock clock_;
    unsigned rate_ = 44100, channels_ = 2, base_clock_ = 7170000;
    std::size_t capacity_frames_ = 0;
    std::uint64_t remainder_ = 0;

  public:
    void set_extra_source(std::function<void(float*, std::size_t, unsigned)> source) {
        extra_source_ = std::move(source);
    }
    std::function<void(float*, std::size_t, unsigned)> extra_source() const {
        return extra_source_;
    }
    PsgSynth::State phase_state() const {
        return synth_.phase_state();
    }
    unsigned rate() const {
        return rate_;
    }
    unsigned base_clock() const {
        return base_clock_;
    }
    std::uint32_t sample_remainder() const {
        return static_cast<std::uint32_t>(remainder_);
    }
    void restore_phase(const PsgSynth::State& state) {
        synth_.restore_phase(state);
    }
    void restore_clock(unsigned rate, unsigned base, std::uint32_t remainder) {
        remainder_ = rate == rate_ && base == base_clock_ && remainder < base ? remainder : 0;
    }
    void configure(unsigned rate, unsigned channels, unsigned base_clock, const PsgState& state) {
        if (!rate || !base_clock || (channels != 1 && channels != 2))
            throw std::invalid_argument("Invalid audio format");
        rate_ = rate;
        channels_ = channels;
        base_clock_ = base_clock;
        capacity_frames_ = (rate * 200u + 999) / 1000;
        ready_.reset(capacity_frames_ * channels);
        samples_.resize(capacity_frames_ * channels);
        extra_samples_.resize(samples_.size());
        reset(0, state);
    }
    void reset(std::uint32_t cycles, const PsgState& state) {
        clock_.reset(cycles);
        remainder_ = 0;
        state_ = state;
        synth_.reset(state);
        ready_.clear();
    }
    void sync(std::uint32_t cycles) {
        if (!capacity_frames_)
            return;
        const auto previous = clock_.elapsed();
        if (clock_.update(cycles)) {
            reset(cycles, state_);
            return;
        }
        auto remaining = clock_.elapsed() - previous;
        std::size_t used = 0;
        auto flush = [&] {
            if (!used)
                return;
            if (extra_source_) {
                extra_source_(extra_samples_.data(), used, channels_);
                for (std::size_t n = 0; n < used * channels_; ++n)
                    samples_[n] += extra_samples_[n];
            }
            std::size_t offset = 0;
            ready_.append(used * channels_, [&](float* out, std::size_t count) {
                std::copy_n(samples_.data() + offset, count, out);
                offset += count;
            });
            used = 0;
        };
        while (remaining) {
            const auto to_sample = (base_clock_ - remainder_ + rate_ - 1) / rate_;
            const auto ticks = (std::min)(remaining, to_sample);
            synth_.advance(ticks);
            remaining -= ticks;
            remainder_ += ticks * rate_;
            while (remainder_ >= base_clock_) {
                remainder_ -= base_clock_;
                synth_.sample(samples_.data() + used * channels_, channels_);
                if (++used == capacity_frames_)
                    flush();
            }
        }
        flush();
    }
    void update(std::uint32_t cycles, const PsgState& state) {
        sync(cycles);
        state_ = state;
        synth_.update(state);
    }
    void mix(float* out, std::size_t frames) {
        if (frames)
            ready_.read(out, frames * channels_);
    }
};
} // namespace rxpce_audio
