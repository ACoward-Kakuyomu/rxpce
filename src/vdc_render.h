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
bool SpritesCollide(const Machine&);
} // namespace rxpce_machine
