#pragma once
#include "video_layout.h"
#include <cstdint>
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace rxpce_video {
struct PresentationFrame {
    std::vector<std::uint32_t> pixels;
    int width = 0, height = 0, scale = 1;
    bool nonblocking = false;
    AspectMode aspect_mode = AspectMode::square_pixels;
    // Assigned by submit; zero is reserved for unqueued/preview images.
    std::uint64_t generation = 0;
};
struct PresentationStatistics {
    std::uint64_t present_us = 0, replaced = 0;
};
class FramePresenter {
  public:
    // One producer; lifecycle operations run after that producer has joined.
    // Render owns its image until completion. Pending frames keep only the latest copy.
    // true completes this image; false retains it for a bounded-rate retry.
    explicit FramePresenter(std::function<bool(const PresentationFrame&)> render);
    ~FramePresenter();
    FramePresenter(const FramePresenter&) = delete;
    FramePresenter& operator=(const FramePresenter&) = delete;
    bool submit(const PresentationFrame& frame);
    // Services synchronous window messages while joining (UI thread lifecycle).
    void stop();
    PresentationStatistics take_statistics();

  private:
    void run() noexcept;
    std::function<bool(const PresentationFrame&)> render_;
    std::mutex mutex_;
    std::condition_variable changed_;
    PresentationFrame pending_;
    PresentationStatistics statistics_;
    std::exception_ptr failure_;
    bool ready_ = false;
    bool stopped_ = false;
    std::uint64_t next_generation_ = 0;
    std::thread worker_;
};
} // namespace rxpce_video
