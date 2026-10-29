#pragma once
#include <windows.h>
#include <cstdint>
#include <memory>
#include <vector>
#include "video_layout.h"

namespace rxpce_video {
class Renderer {
  public:
    Renderer();
    ~Renderer();
    HRESULT initialize(HWND window, int adapter);
    void shutdown();
    // Square pixels: scale 0 fits the largest integer enlargement.
    // TV mode: fit a 4:3 rectangle to the client area at any valid scale.
    // nonblocking selects a zero (rather than 1 ms) slot-acquisition timeout.
    // Both modes require a slot and submit Present without blocking.
    HRESULT render(const std::uint32_t* rgba, int width, int height, int scale, bool present = true,
                   bool nonblocking = false, AspectMode aspect_mode = AspectMode::square_pixels);
    // Read the last drawn target before Present (used by the WARP regression test).
    HRESULT readback(std::vector<std::uint32_t>& rgba);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace rxpce_video
