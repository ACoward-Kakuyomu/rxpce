#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>

namespace rxpce_audio {
// Output-only data, excluded from all machine/audio snapshots.
struct OutputSample {
    float legacy = 0, psg = 0;
    double cdda = 0, adpcm = 0;
};
class OutputGain {
    struct Ramp {
        double value = 1, target = 1, step = 0;
        unsigned remaining = 0;
        void set(double next, unsigned frames) {
            if (target == next) return;
            target = next;
            remaining = frames;
            step = (target-value)/frames;
        }
        void advance() {
            if (remaining && --remaining == 0) value = target;
            else if (remaining) value += step;
        }
    };
    std::array<Ramp,4> ramps_{};
    unsigned rate_ = 44100;
  public:
    bool set(unsigned master, unsigned psg, unsigned cdda, unsigned adpcm, bool muted) {
        if (master>100 || psg>100 || cdda>100 || adpcm>100) return false;
        const unsigned frames = static_cast<unsigned>((std::uint64_t(rate_)*5+999)/1000);
        const double levels[] = {muted ? 0 : master/100.0, psg/100.0, cdda/100.0, adpcm/100.0};
        for (unsigned n=0;n<4;++n) ramps_[n].set(levels[n],frames);
        return true;
    }
    void configure(unsigned rate) {
        if (rate < 1000 || rate > 192000)
            throw std::invalid_argument("Invalid output gain rate");
        rate_ = rate;
        for (auto& ramp : ramps_) { ramp.value = ramp.target; ramp.remaining = 0; ramp.step = 0; }
    }
    void next_frame() { for (auto& ramp : ramps_) ramp.advance(); }
    float apply(const OutputSample& sample) const {
        const double master = ramps_[0].value;
        if (master == 0) return 0;
        if (ramps_[1].value == 1 && ramps_[2].value == 1 && ramps_[3].value == 1)
            return master == 1 ? sample.legacy : static_cast<float>(sample.legacy*master);
        return static_cast<float>((sample.psg*ramps_[1].value + sample.cdda*ramps_[2].value +
                                   sample.adpcm*ramps_[3].value)*master);
    }
};
}
