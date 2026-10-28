#pragma once
#include "psg_state.h"

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
    std::uint64_t frames(unsigned rate, unsigned clock) const {
        // Divide first to keep long sessions within the 64-bit product range.
        return cycles_ / clock * rate + cycles_ % clock * rate / clock;
    }
};

class PsgSynth {
  public:
    struct Voice {
        std::uint32_t index = 0, noise = 0, seed = 0;
        std::uint64_t phase = 0;
    };
    using State = std::array<Voice, 6>;
    State phase_state() const {
        return voices_;
    }
    void restore_phase(const State& state, unsigned old_rate, unsigned new_rate) {
        voices_ = state;
        for (auto& voice : voices_)
            voice.phase = voice.phase * new_rate / old_rate;
    }

  private:
    std::array<Voice, 6> voices_{};

    static float gain(int index) {
        static const int table[32] = {100,  451,  508,  573,   646,   728,   821,   925,
                                      1043, 1175, 1325, 1493,  1683,  1898,  2139,  2411,
                                      2718, 3064, 3454, 3893,  4388,  4947,  5576,  6285,
                                      7085, 7986, 9002, 10148, 11439, 12894, 14535, 16384};
        index = (std::max)(0, (std::min)(31, index - 60));
        return table[index] / 16384.0f;
    }

  public:
    PsgSynth() {
        reset();
    }
    void reset() {
        voices_ = {};
        voices_[4].seed = voices_[5].seed = 0x51f631e4u;
    }
    void render(const PsgState& state, int ch, float* out, std::size_t frames, unsigned channels,
                unsigned rate) {
        auto& voice = voices_[ch];
        const auto& reg = state.registers[ch];
        if (!(reg[4] & 0x80)) {
            voice.index = 0;
            voice.phase = 0;
            std::fill_n(out, frames * channels, 0.0f);
            return;
        }
        const int left_main = (state.volume >> 3) & 0x1e;
        const int right_main = (state.volume << 1) & 0x1e;
        const int left_ch = (reg[5] >> 3) & 0x1e;
        const int right_ch = (reg[5] << 1) & 0x1e;
        const int volume = reg[4] & 0x1f;
        const float left =
            channels == 2
                ? gain(left_main + volume + left_ch)
                : gain((std::max)(left_main, right_main) + volume + (std::max)(left_ch, right_ch));
        const float right = gain(right_main + volume + right_ch);
        const bool dda = (reg[4] & 0x40) != 0;
        const bool noise = ch >= 4 && (reg[7] & 0x80) != 0;
        const unsigned period = reg[2] + ((reg[3] & 15u) << 8);
        for (std::size_t frame = 0; frame < frames; ++frame) {
            float sample = 0;
            if (dda) {
                sample = (static_cast<int>(state.wave[ch][0]) - 16) * (702.0f / 32768.0f);
            } else if (noise) {
                voice.phase += 3000 + (reg[7] & 0x1f) * 512;
                if (voice.phase >= rate) {
                    const bool bit = (voice.seed & 0x00080000) != 0;
                    voice.seed = bit ? ((voice.seed ^ 4u) << 1) + 1 : voice.seed << 1;
                    voice.noise = bit;
                    voice.phase %= rate;
                }
                sample = (voice.noise ? 10 : -10) * (702.0f / 32768.0f);
            } else if (period) {
                sample = (static_cast<int>(state.wave[ch][voice.index]) - 16) * (702.0f / 32768.0f);
                voice.phase += 32ull * 1118608 / period;
                const auto step = voice.phase / (10ull * rate);
                voice.index = static_cast<std::uint32_t>((voice.index + step) % 32);
                voice.phase %= 10ull * rate;
            }
            *out++ = sample * left;
            if (channels == 2)
                *out++ = sample * right;
        }
    }
};

class Mixer {
    std::function<void(float*, std::size_t, unsigned)> extra_source_;
    PsgState state_;
    PsgSynth synth_;
    SampleQueue ready_;
    std::vector<float> voice_samples_;
    AudioClock clock_;
    unsigned rate_ = 44100, channels_ = 2, base_clock_ = 7170000;
    std::size_t capacity_frames_ = 0;
    std::uint64_t generated_ = 0;

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
    void restore_phase(const PsgSynth::State& state, unsigned old_rate) {
        synth_.restore_phase(state, old_rate, rate_);
    }
    void configure(unsigned rate, unsigned channels, unsigned base_clock, const PsgState& state) {
        if (!rate || !base_clock || (channels != 1 && channels != 2))
            throw std::invalid_argument("Invalid audio format");
        rate_ = rate;
        channels_ = channels;
        base_clock_ = base_clock;
        capacity_frames_ = (rate * 200u + 999) / 1000;
        ready_.reset(capacity_frames_ * channels);
        voice_samples_.resize(capacity_frames_ * channels);
        reset(0, state);
    }
    void reset(std::uint32_t cycles, const PsgState& state) {
        clock_.reset(cycles);
        generated_ = 0;
        state_ = state;
        synth_.reset();
        ready_.clear();
    }
    void sync(std::uint32_t cycles) {
        if (!capacity_frames_)
            return;
        if (clock_.update(cycles)) {
            reset(cycles, state_);
            return;
        }
        const auto target = clock_.frames(rate_, base_clock_);
        while (generated_ < target) {
            const auto frames = static_cast<std::size_t>(
                (std::min)(target - generated_, static_cast<std::uint64_t>(capacity_frames_)));
            ready_.append(frames * channels_, [&](float* out, std::size_t samples) {
                std::fill_n(out, samples, 0.0f);
                for (int ch = 0; ch < 6; ++ch) {
                    synth_.render(state_, ch, voice_samples_.data(), samples / channels_, channels_,
                                  rate_);
                    for (std::size_t i = 0; i < samples; ++i)
                        out[i] += voice_samples_[i];
                }
                if (extra_source_) {
                    extra_source_(voice_samples_.data(), samples / channels_, channels_);
                    for (std::size_t i = 0; i < samples; ++i)
                        out[i] += voice_samples_[i];
                }
            });
            generated_ += frames;
        }
    }
    void update(std::uint32_t cycles, const PsgState& state) {
        sync(cycles); // Finish every voice with the old state before changing registers.
        state_ = state;
    }
    void mix(float* out, std::size_t frames) {
        if (!frames)
            return;
        ready_.read(out, frames * channels_);
    }
};

} // namespace rxpce_audio
