#pragma once
#include "video_frame.h"
#include "sprite_evaluation.h"
#include <cstddef>

namespace rxpce_machine {
class Machine;
struct FrameSurface {
    rxpce_video::Pixel* pixels;
    std::size_t count;
};
struct BackgroundRegisters {
    std::uint16_t control, x_scroll;
    int y_offset;
};
struct SpriteDisplay {
    int row, first, last, width;
};
// Background and clear use inclusive last rows; sprite lines use explicit plans.
void RenderBackground(Machine&, FrameSurface, int first, int last, const BackgroundRegisters&);
void ClearVideoRows(Machine&, FrameSurface, int first, int last);
// Select a row using the active VDC mode and horizontal fetch limits.
rxpce_video::SpriteLinePlan EvaluateMachineSprites(const Machine&, int row);
void RenderSpriteLine(Machine&, FrameSurface, int row, unsigned priority,
                      const rxpce_video::SpriteLinePlan&);
// Evaluate opaque sprite-zero overlap on Machine::scanline in the VDC display area.
bool SpritesCollide(const Machine&, const rxpce_video::SpriteLinePlan&, const SpriteDisplay&);
} // namespace rxpce_machine
