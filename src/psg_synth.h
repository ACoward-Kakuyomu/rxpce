#pragma once
#include "psg_state.h"
#include <algorithm>
#include <array>
#include <cstdint>

namespace rxpce_audio {
// Original clock-domain model. All counter units are CPU high-speed clocks.
class PsgSynth {
  public:
    struct Voice {
        std::uint32_t index = 0, seed = 1, tone_remaining = 0, noise_remaining = 0;
        bool operator==(const Voice& other) const {
            return index == other.index && seed == other.seed &&
                   tone_remaining == other.tone_remaining &&
                   noise_remaining == other.noise_remaining;
        }
    };
    using State = std::array<Voice, 6>;
    State phase_state() const {
        return voices_;
    }
    void restore_phase(const State& state) {
        voices_ = state;
    }
    PsgSynth() {
        reset();
    }
    void reset(const PsgState& state = {}) {
        registers_ = state;
        voices_ = {};
        for (unsigned ch = 0; ch < 6; ++ch) {
            voices_[ch].tone_remaining = tone_period(ch);
            if (ch >= 4)
                voices_[ch].noise_remaining = noise_period(ch);
        }
    }
    void update(const PsgState& next) {
        const bool reset_held = (registers_.lfo_control & 0x80) != 0;
        std::array<std::uint8_t, 6> previous;
        for (unsigned ch = 0; ch < 6; ++ch)
            previous[ch] = registers_.registers[ch][4];
        registers_ = next;
        for (unsigned ch = 0; ch < 6; ++ch) {
            const auto old = previous[ch], control = next.registers[ch][4];
            if ((old ^ control) & 0x40) {
                voices_[ch].index = 0;
                voices_[ch].tone_remaining = tone_period(ch);
            }
            if (!(old & 0x80) && (control & 0x80))
                voices_[ch].tone_remaining = tone_period(ch);
        }
        if (next.lfo_control & 0x80) {
            voices_[1].index = 0;
            voices_[1].tone_remaining = tone_period(1);
        } else if (reset_held) {
            voices_[1].tone_remaining = tone_period(1);
        }
    }
    void advance(std::uint64_t clocks) {
        for (unsigned ch = 4; ch < 6; ++ch) {
            const auto shifts = steps(voices_[ch].noise_remaining, clocks, noise_period(ch));
            for (std::uint64_t n = 0; n < shifts; ++n) {
                const auto seed = voices_[ch].seed;
                const auto feedback =
                    ((seed >> 0) ^ (seed >> 1) ^ (seed >> 11) ^ (seed >> 12) ^ (seed >> 17)) & 1;
                voices_[ch].seed = (seed >> 1) | (feedback << 17);
            }
        }
        if (lfo_active() && !(registers_.lfo_control & 0x80)) {
            auto remaining = clocks;
            auto& destination = voices_[0];
            auto& source = voices_[1];
            if (!source.tone_remaining)
                source.tone_remaining = tone_period(1);
            if (!destination.tone_remaining)
                destination.tone_remaining = tone_period(0);
            while (remaining) {
                const auto slice =
                    (std::min)(remaining, std::uint64_t((std::min)(source.tone_remaining,
                                                                   destination.tone_remaining)));
                remaining -= slice;
                source.tone_remaining -= static_cast<std::uint32_t>(slice);
                destination.tone_remaining -= static_cast<std::uint32_t>(slice);
                if (!source.tone_remaining) {
                    source.index = (source.index + 1) & 31;
                    source.tone_remaining = tone_period(1);
                }
                if (!destination.tone_remaining) {
                    destination.index = (destination.index + 1) & 31;
                    destination.tone_remaining = tone_period(0);
                }
            }
        } else {
            advance_tone(0, clocks);
            if (!(registers_.lfo_control & 0x80))
                advance_tone(1, clocks);
        }
        for (unsigned ch = 2; ch < 6; ++ch)
            advance_tone(ch, clocks);
    }
    void sample(float* out, unsigned channels) const {
        float left = 0, right = 0;
        for (unsigned ch = 0; ch < 6; ++ch) {
            const auto& reg = registers_.registers[ch];
            if (!(reg[4] & 0x80))
                continue;
            int data;
            if (ch >= 4 && (reg[7] & 0x80))
                data = (voices_[ch].seed & 1) ? 31 : 0;
            else if (reg[4] & 0x40)
                data = reg[6] & 31;
            else
                data = registers_.wave[ch][voices_[ch].index] & 31;
            const float value = (data - 16) * (702.0f / 32768.0f);
            const int lm = (registers_.volume >> 3) & 30, rm = (registers_.volume << 1) & 30;
            const int lc = (reg[5] >> 3) & 30, rc = (reg[5] << 1) & 30;
            const int volume = reg[4] & 31;
            left += value * gain(channels == 2 ? lm + volume + lc
                                               : (std::max)(lm, rm) + volume + (std::max)(lc, rc));
            right += value * gain(rm + volume + rc);
        }
        out[0] = left;
        if (channels == 2)
            out[1] = right;
    }

  private:
    State voices_{};
    PsgState registers_;
    bool wave_on(unsigned ch) const {
        return (registers_.registers[ch][4] & 0xc0) == 0x80;
    }
    bool lfo_active() const {
        return (registers_.lfo_control & 3) && wave_on(0) && wave_on(1);
    }
    std::uint32_t tone_period(unsigned ch) const {
        const auto& reg = registers_.registers[ch];
        int frequency = reg[2] | ((reg[3] & 15) << 8);
        if (ch == 0 && lfo_active()) {
            const int multiplier[] = {0, 1, 4, 16};
            frequency += (int(registers_.wave[1][voices_[1].index] & 31) - 16) *
                         multiplier[registers_.lfo_control & 3];
            frequency = int(std::uint32_t(frequency) & 4095);
        }
        std::uint32_t period = 2u * (frequency ? frequency : 4096);
        if (ch == 1 && (registers_.lfo_control & 3))
            period *= registers_.lfo_frequency ? registers_.lfo_frequency : 256;
        return period;
    }
    std::uint32_t noise_period(unsigned ch) const {
        const unsigned divisor = 31 - (registers_.registers[ch][7] & 31);
        return divisor ? divisor * 128 : 64;
    }
    static std::uint64_t steps(std::uint32_t& remaining, std::uint64_t clocks,
                               std::uint32_t period) {
        if (!remaining)
            remaining = period;
        if (clocks < remaining) {
            remaining -= static_cast<std::uint32_t>(clocks);
            return 0;
        }
        const auto after = clocks - remaining;
        remaining = period - static_cast<std::uint32_t>(after % period);
        return 1 + after / period;
    }
    void advance_tone(unsigned ch, std::uint64_t clocks) {
        if (!wave_on(ch))
            return;
        auto& voice = voices_[ch];
        voice.index = static_cast<std::uint32_t>(
            (voice.index + steps(voice.tone_remaining, clocks, tone_period(ch))) & 31);
    }
    static float gain(int index) {
        static const int table[32] = {100,  451,  508,  573,   646,   728,   821,   925,
                                      1043, 1175, 1325, 1493,  1683,  1898,  2139,  2411,
                                      2718, 3064, 3454, 3893,  4388,  4947,  5576,  6285,
                                      7085, 7986, 9002, 10148, 11439, 12894, 14535, 16384};
        index = (std::max)(0, (std::min)(31, index - 60));
        return table[index] / 16384.0f;
    }
};
} // namespace rxpce_audio
