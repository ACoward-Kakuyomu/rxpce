#pragma once
#include "psg_state.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <cmath>

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
            rebuild_changes(ch, state.wave[ch]);
            voices_[ch].tone_remaining = tone_period(ch);
            if (ch >= 4)
                voices_[ch].noise_remaining = noise_period(ch);
        }
        rebuild_gains();
        output_constant_ = compute_constant();
    }
    void update(const PsgState& next) {
        const bool reset_held = (registers_.lfo_control & 0x80) != 0;
        std::array<std::uint8_t, 6> previous;
        for (unsigned ch = 0; ch < 6; ++ch) {
            previous[ch] = registers_.registers[ch][4];
            if (registers_.wave[ch] != next.wave[ch])
                rebuild_changes(ch, next.wave[ch]);
        }
        registers_ = next;
        for (unsigned ch = 0; ch < 6; ++ch) {
            const auto old = previous[ch], control = next.registers[ch][4];
            if ((old ^ control) & 0x40) {
                voices_[ch].index = 0;
                voices_[ch].tone_remaining = tone_period(ch);
            }
            if (control & 0x40)
                voices_[ch].index = 0;
            if (!(old & 0x80) && (control & 0x80))
                voices_[ch].tone_remaining = tone_period(ch);
        }
        if (next.lfo_control & 0x80) {
            voices_[1].index = 0;
            voices_[1].tone_remaining = tone_period(1);
        } else if (reset_held) {
            voices_[1].tone_remaining = tone_period(1);
        }
        rebuild_gains();
        output_constant_ = compute_constant();
    }
    const PsgState& register_state() const {
        return registers_;
    }
    void write(unsigned reg, std::uint8_t value) {
        reg &= 15;
        if (reg == 0) {
            registers_.selected = value & 7;
            return;
        }
        const unsigned ch = registers_.selected;
        if (reg >= 2 && reg <= 7 && ch >= 6)
            return;
        auto next = registers_;
        bool wave_write = false;
        unsigned address = 0;
        switch (reg) {
        case 1:
            next.volume = value;
            break;
        case 2:
            next.registers[ch][2] = value;
            break;
        case 3:
            next.registers[ch][3] = value & 15;
            break;
        case 4:
            next.registers[ch][4] = value;
            break;
        case 5:
            next.registers[ch][5] = value;
            break;
        case 6:
            if (next.registers[ch][4] & 0x40)
                next.registers[ch][6] = value & 31;
            else if (!(next.registers[ch][4] & 0x80)) {
                address = voices_[ch].index;
                next.wave[ch][address] = value & 31;
                wave_write = true;
            }
            break;
        case 7:
            next.registers[ch][7] = value;
            break;
        case 8:
            next.lfo_frequency = value;
            break;
        case 9:
            next.lfo_control = value;
            break;
        default:
            return;
        }
        update(next);
        if (wave_write)
            voices_[ch].index = (address + 1) & 31;
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
    bool constant_output() const {
        return output_constant_;
    }
    template <class Emit>
    void advance_events(std::uint64_t clocks, Emit emit, unsigned channels = 2) {
        if (!clocks)
            return;
        auto next = next_output_change();
        if (next > clocks) {
            advance(clocks);
            return;
        }
        std::array<float, 2> previous{};
        sample(previous.data(), channels);
        std::uint64_t elapsed = 0;
        while (elapsed < clocks) {
            advance(next);
            elapsed += next;
            std::array<float, 2> current{};
            sample(current.data(), channels);
            if (current != previous)
                emit(elapsed, current);
            previous = current;
            if (elapsed == clocks)
                break;
            next = next_output_change();
            if (next > clocks - elapsed) {
                advance(clocks - elapsed);
                break;
            }
        }
    }
    void sample(float* out, unsigned channels) const {
        float left = 0, right = 0;
        for (unsigned ch = 0; ch < 6; ++ch) {
            const auto& reg = registers_.registers[ch];
            if (!(reg[4] & 0x80) || (gain_left_[ch] == 0 && gain_right_[ch] == 0))
                continue;
            int data;
            if (ch >= 4 && (reg[7] & 0x80))
                data = (voices_[ch].seed & 1) ? 31 : 0;
            else if (reg[4] & 0x40)
                data = reg[6] & 31;
            else
                data = registers_.wave[ch][voices_[ch].index] & 31;
            const float value = (data - 16) * (702.0f / 32768.0f);
            left += value * gain_left_[ch];
            right += value * gain_right_[ch];
        }
        out[0] = channels == 1 ? (left + right) * 0.5f : left;
        if (channels == 2)
            out[1] = right;
    }

  private:
    State voices_{};
    PsgState registers_;
    bool output_constant_ = true;
    std::array<float, 6> gain_left_{}, gain_right_{};
    std::array<std::array<std::uint8_t, 32>, 6> changes_{};
    bool compute_constant() const {
        for (unsigned ch = 0; ch < 6; ++ch) {
            const auto control = registers_.registers[ch][4];
            if (!(control & 0x80) || (gain_left_[ch] == 0 && gain_right_[ch] == 0))
                continue;
            if (ch >= 4 && (registers_.registers[ch][7] & 0x80))
                return false;
            if (wave_on(ch) && changes_[ch][0] && !(ch == 1 && (registers_.lfo_control & 0x80)))
                return false;
        }
        return true;
    }
    std::uint64_t next_output_change() const {
        auto next = (std::numeric_limits<std::uint64_t>::max)();
        for (unsigned ch = 0; ch < 6; ++ch) {
            const auto& reg = registers_.registers[ch];
            if (!(reg[4] & 0x80) || (gain_left_[ch] == 0 && gain_right_[ch] == 0))
                continue;
            if (ch >= 4 && (reg[7] & 0x80)) {
                const auto due = voices_[ch].noise_remaining;
                next = (std::min)(next, std::uint64_t(due ? due : noise_period(ch)));
                continue;
            }
            if (!wave_on(ch) || (ch == 1 && (registers_.lfo_control & 0x80)))
                continue;
            const auto changes = changes_[ch][voices_[ch].index];
            if (!changes)
                continue;
            const auto period = tone_period(ch), due = voices_[ch].tone_remaining;
            const auto distance = ch == 0 && lfo_active() ? 1 : changes;
            const auto deadline =
                std::uint64_t(due ? due : period) + std::uint64_t(distance - 1) * period;
            next = (std::min)(next, deadline);
        }
        return next;
    }
    void rebuild_changes(unsigned ch, const std::array<std::uint8_t, 32>& wave) {
        const auto first = wave[0] & 31;
        if (std::all_of(wave.begin() + 1, wave.end(),
                        [first](std::uint8_t value) { return (value & 31) == first; })) {
            changes_[ch].fill(0);
            return;
        }
        for (unsigned index = 0; index < 32; ++index) {
            changes_[ch][index] = 0;
            for (unsigned distance = 1; distance < 32; ++distance)
                if ((wave[(index + distance) & 31] & 31) != (wave[index] & 31)) {
                    changes_[ch][index] = static_cast<std::uint8_t>(distance);
                    break;
                }
        }
    }
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
    static float side_gain(unsigned main, unsigned balance, unsigned level) {
        static const auto table = [] {
            std::array<float, 32> result{};
            for (unsigned loss = 0; loss < 31; ++loss)
                result[loss] = static_cast<float>(std::pow(10.0, -1.5 * loss / 20));
            return result;
        }();
        if (!main || !balance || !level)
            return 0;
        const unsigned loss = 31 - level + 2 * (15 - main) + 2 * (15 - balance);
        return table[(std::min)(31u, loss)];
    }
    void rebuild_gains() {
        for (unsigned ch = 0; ch < 6; ++ch) {
            const auto& reg = registers_.registers[ch];
            gain_left_[ch] = side_gain(registers_.volume >> 4, reg[5] >> 4, reg[4] & 31);
            gain_right_[ch] = side_gain(registers_.volume & 15, reg[5] & 15, reg[4] & 31);
        }
    }
};
} // namespace rxpce_audio
