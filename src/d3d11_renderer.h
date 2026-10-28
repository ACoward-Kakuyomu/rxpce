#pragma once
#include <windows.h>
#include <cstdint>
#include <memory>
#include <vector>

namespace rxpce_video {
class Renderer {
  public:
    Renderer();
    ~Renderer();
    HRESULT initialize(HWND window, int adapter);
    void shutdown();
    // scale 0 fits the largest integer enlargement to the client area.
    // The final flag selects a zero (rather than 1 ms) slot-acquisition timeout.
    // Both modes require a slot and submit Present without blocking.
    HRESULT render(const std::uint32_t* rgba, int width, int height, int scale, bool present = true,
                   bool nonblocking = false);
    // Read the last drawn target before Present (used by the WARP regression test).
    HRESULT readback(std::vector<std::uint32_t>& rgba);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace rxpce_video
