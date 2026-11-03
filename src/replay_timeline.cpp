#include "replay_timeline.h"
#include <stdexcept>

namespace rxpce_replay {
namespace {
constexpr std::size_t checkpoint_interval = 120;
void add_checkpoint(Recording& recording, std::uint64_t time, const DigestProvider& digest) {
    recording.checkpoints.push_back(
        {static_cast<std::uint32_t>(recording.inputs.size()), time, digest()});
}
} // namespace
void append_input(Recording& recording, std::uint64_t time, const std::function<Buttons()>& sample,
                  const DigestProvider& digest) {
    const auto count = recording.inputs.size();
    if (count >= max_inputs)
        throw std::length_error("Replay input limit reached");
    if (count && count % checkpoint_interval == 0) {
        const auto value = digest();
        recording.inputs.push_back({time, sample()});
        recording.checkpoints.push_back({static_cast<std::uint32_t>(count), time, value});
    } else {
        recording.inputs.push_back({time, sample()});
    }
}
void finish_recording(Recording& recording, std::uint64_t time, const DigestProvider& digest) {
    if (recording.inputs.empty())
        throw std::invalid_argument("Replay has no recorded input");
    add_checkpoint(recording, time, digest);
}
PlaybackResult Playback::step(std::uint64_t time, Buttons& buttons, const DigestProvider& digest,
                              std::string& error) {
    if (result_ != PlaybackResult::applied)
        return result_;
    const bool terminal = update_ == recording_.inputs.size();
    const bool check = checkpoint_ < recording_.checkpoints.size() &&
                       recording_.checkpoints[checkpoint_].updates == update_;
    const auto mismatch = [&](const char* detail, std::uint64_t expected) {
        error = std::string(detail) + " at update " + std::to_string(update_) +
                ", master clock " + std::to_string(time) +
                " (expected " + std::to_string(expected) + ")";
        result_ = PlaybackResult::mismatch;
        return result_;
    };
    if (!terminal && recording_.inputs[update_].time != time)
        return mismatch("Replay input timing mismatch", recording_.inputs[update_].time);
    if (check) {
        const auto& point = recording_.checkpoints[checkpoint_];
        if (point.time != time)
            return mismatch("Replay checkpoint timing mismatch", point.time);
        if (point.digest != digest())
            return mismatch("Replay state mismatch", point.time);
        ++checkpoint_;
    }
    error.clear();
    if (terminal) {
        result_ = PlaybackResult::complete;
    } else {
        buttons = recording_.inputs[update_++].buttons;
    }
    return result_;
}
} // namespace rxpce_replay
