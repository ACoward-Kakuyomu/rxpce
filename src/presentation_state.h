#pragma once
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
    void submitted(PresentResult result) {
        action = result == PresentResult::busy       ? PresentAction::retry
                 : result == PresentResult::occluded ? PresentAction::visibility
                                                     : PresentAction::acquire;
    }
    void visible(bool visible) {
        if (action == PresentAction::visibility && visible)
            action = PresentAction::draw;
    }

  private:
    PresentAction action = PresentAction::acquire;
};
} // namespace rxpce_video
