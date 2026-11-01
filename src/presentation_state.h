#pragma once
#include <cstdint>
namespace rxpce_video {
enum class PresentAction { acquire, draw, retry, visibility };
enum class PresentResult { presented, busy, occluded };

// One acquired slot belongs to one submitted frame, independently of CPU speed.
class PresentationState {
  public:
    PresentAction next() const {
        return action;
    }
    bool acquire(bool ready) {
        if (action != PresentAction::acquire || !ready)
            return false;
        action = PresentAction::draw;
        return true;
    }
    bool drawn(std::uint64_t image) {
        if (action != PresentAction::draw)
            return false;
        image_ = image;
        prepared_ = true;
        return true;
    }
    bool submitted(PresentResult result, std::uint64_t requested = 0) {
        const bool complete =
            result == PresentResult::presented && prepared_ && image_ == requested;
        if (result != PresentResult::busy)
            prepared_ = false;
        action = result == PresentResult::busy       ? PresentAction::retry
                 : result == PresentResult::occluded ? PresentAction::visibility
                                                     : PresentAction::acquire;
        return complete;
    }
    void visible(bool visible) {
        if (action == PresentAction::visibility && visible)
            action = PresentAction::draw;
    }

  private:
    PresentAction action = PresentAction::acquire;
    std::uint64_t image_ = 0;
    bool prepared_ = false;
};
} // namespace rxpce_video
