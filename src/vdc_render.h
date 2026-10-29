#pragma once
#include "video_frame.h"
#include <cstddef>

namespace rxpce_machine {
class Machine;
struct FrameSurface {
    rxpce_video::Pixel* pixels;
    std::size_t count;
};
// Background and clear use inclusive last rows; sprites use an exclusive end.
void RenderBackground(Machine&, FrameSurface, int first, int last);
void RenderSprites(Machine&, FrameSurface, int first, int end, unsigned priority);
void ClearVideoRows(Machine&, FrameSurface, int first, int last);
// Evaluate opaque sprite-zero overlap on Machine::scanline in the VDC display area.
bool SpritesCollide(const Machine&);
} // namespace rxpce_machine
