#pragma once
#include "replay_codec.h"
#include <functional>

namespace rxpce_replay {
using DigestProvider = std::function<Digest()>;
void append_input(Recording& recording, std::uint64_t time, const std::function<Buttons()>& sample,
                  const DigestProvider& digest);
void finish_recording(Recording& recording, std::uint64_t time, const DigestProvider& digest);

enum class PlaybackResult { applied, complete, mismatch };
class Playback {
  public:
    // The recording must outlive the playback and remain unchanged.
    explicit Playback(const Recording& recording) : recording_(recording) {}
    PlaybackResult step(std::uint64_t time, Buttons& buttons, const DigestProvider& digest,
                        std::string& error);

  private:
    const Recording& recording_;
    std::size_t update_ = 0, checkpoint_ = 0;
    PlaybackResult result_ = PlaybackResult::applied;
};
} // namespace rxpce_replay
