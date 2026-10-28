#pragma once
#include <algorithm>
#include <cstdint>

namespace rxpce_audio {

inline std::uint32_t stream_capacity_frames(unsigned rate, unsigned latency_ms) {
    const auto requested = static_cast<std::uint64_t>(rate) * latency_ms / 1000;
    return static_cast<std::uint32_t>(
        (std::max)(static_cast<std::uint64_t>(rate) / 5, requested * 2 + 1));
}

// All positions are complete PCM frames. Keep one frame free to distinguish
// a full ring from an empty ring without using a cursor-distance heuristic.
class PlaybackCursor {
    std::uint32_t capacity_ = 0, last_play_ = 0;
    std::uint32_t target_ = 0;
    std::uint32_t max_step_ = 0;
    bool seen_cursor_ = false;
    std::uint64_t played_ = 0, written_ = 0, safe_ = 0;

  public:
    void reset(std::uint32_t capacity, std::uint32_t target = 0) {
        capacity_ = capacity;
        last_play_ = 0;
        played_ = written_ = safe_ = 0;
        max_step_ = 0;
        seen_cursor_ = false;
        target_ = target ? target : (capacity ? capacity - 1 : 0);
    }
    std::uint32_t capacity() const {
        return capacity_;
    }
    bool poll(std::uint32_t play, std::uint32_t safe_write) {
        const auto step = seen_cursor_ ? (play + capacity_ - last_play_) % capacity_ : 0;
        if (!seen_cursor_) {
            played_ = play;
            seen_cursor_ = true;
        }
        max_step_ = (std::max)(max_step_, step);
        played_ += step;
        last_play_ = play;
        safe_ = played_ + (safe_write + capacity_ - play) % capacity_;
        if (written_ < safe_) {
            written_ = safe_;
            return true;
        }
        return false;
    }
    std::uint32_t offset() const {
        return static_cast<std::uint32_t>(written_ % capacity_);
    }
    std::uint32_t effective_lead(std::uint32_t block) const {
        return static_cast<std::uint32_t>(
            (std::min)(static_cast<std::uint64_t>(capacity_ - 1),
                       (std::max)(static_cast<std::uint64_t>(target_),
                                  safe_ - played_ + (std::max)(block, max_step_))));
    }
    std::uint32_t needed(std::uint32_t block) const {
        const auto end = played_ + effective_lead(block);
        return written_ >= end ? 0
                               : static_cast<std::uint32_t>(
                                     (std::min)(end - written_, static_cast<std::uint64_t>(block)));
    }
    void commit(std::uint32_t frames) {
        written_ += frames;
    }
};

} // namespace rxpce_audio
