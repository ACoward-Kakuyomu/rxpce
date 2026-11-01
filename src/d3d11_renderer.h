#pragma once
#include <windows.h>
#include <cstdint>
#include <memory>
#include <vector>
#include "video_layout.h"

namespace rxpce_video {
struct PresentationFrame;
struct RenderAttempt {
    HRESULT status;
    bool complete;
};
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
    // Requires a nonzero generation assigned by FramePresenter. S_OK may submit
    // an older busy image; complete refers only to the supplied frame.
    RenderAttempt present_frame(const PresentationFrame& frame);
    // Read the last drawn target before Present (used by the WARP regression test).
    HRESULT readback(std::vector<std::uint32_t>& rgba);

  private:
    RenderAttempt render_image(const std::uint32_t* rgba, int width, int height, int scale,
                               bool present, bool nonblocking, AspectMode aspect_mode,
                               std::uint64_t generation);
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace rxpce_video
