#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace rxpce_cd {
class AdpcmDecoder {
    int prediction_ = 0, index_ = 0;

  public:
    int prediction() const {
        return prediction_;
    }
    unsigned index() const {
        return static_cast<unsigned>(index_);
    }
    bool restore(int prediction, unsigned index) {
        if (prediction < -2048 || prediction > 2047 || index > 48)
            return false;
        prediction_ = prediction;
        index_ = static_cast<int>(index);
        return true;
    }
    void reset() {
        prediction_ = index_ = 0;
    }
    int decode(unsigned nibble) {
        // MSM5205: geometric quantizer, separately truncated binary weights.
        static const auto steps = [] {
            std::array<int, 49> table{};
            for (unsigned n = 0; n < table.size(); ++n)
                table[n] = static_cast<int>(16 * std::pow(1.1, n));
            return table;
        }();
        const int step = steps[index_];
        const int delta = step / 8 + ((nibble & 1) ? step / 4 : 0) + ((nibble & 2) ? step / 2 : 0) +
                          ((nibble & 4) ? step : 0);
        prediction_ =
            (std::max)(-2048, (std::min)(2047, prediction_ + ((nibble & 8) ? -delta : delta)));
        const int magnitude = nibble & 7;
        index_ = (std::max)(0, (std::min)(48, index_ + (magnitude < 4 ? -1 : 2 * (magnitude - 3))));
        return prediction_;
    }
};
} // namespace rxpce_cd
