#pragma once
#include "replay_timeline.h"
#include <atomic>
#include <mutex>
#include <optional>
#include <memory>

namespace rxpce_machine {
class Machine;
}
namespace rxpce_replay {
// Input and playback cursors belong to the worker. The UI can observe recording
// status and take a finished recording while that worker is still running.
class Session {
  public:
    explicit Session(std::shared_ptr<const Recording> playback = {},
                     std::function<void()> finished = {})
        : source_(std::move(playback)), notify_finished_(std::move(finished)) {}
    const Recording* source() const noexcept { return source_.get(); }
    bool recording() const noexcept { return recording_.load(); }
    // UI marks intent before posting the worker command. Host-only, never serialized.
    bool request_record_toggle() noexcept {
        if(source_ || finished_ready_.load()) return false;
        return !record_request_pending_.exchange(true);
    }
    bool blocks_media_drop() const noexcept {
        return source_ || record_request_pending_.load() || recording_.load() || finished_ready_.load();
    }
    bool begin_playback(rxpce_machine::Machine& machine);
    bool boundary(rxpce_machine::Machine& machine, bool toggle, bool stopping,
                  bool record_request_seen=false);
    std::optional<Recording> take_finished();

  private:
    std::shared_ptr<const Recording> source_;
    std::function<void()> notify_finished_;
    std::optional<Playback> playback_;
    std::atomic_bool recording_{false};
    std::atomic_bool record_request_pending_{false}, finished_ready_{false};
    Recording draft_;
    std::mutex finished_mutex_;
    std::optional<Recording> finished_;
    bool play_input(rxpce_machine::Machine& machine);
    void finish(rxpce_machine::Machine& machine);
};
} // namespace rxpce_replay
