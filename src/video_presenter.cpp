#include "video_presenter.h"
#include "ui_thread_wait.h"
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <utility>

namespace rxpce_video {
FramePresenter::FramePresenter(std::function<void(const PresentationFrame&)> render)
    : render_(std::move(render)) {
    if (!render_)
        throw std::invalid_argument("Missing presentation callback");
    worker_ = std::thread(&FramePresenter::run, this);
}
FramePresenter::~FramePresenter() {
    stop();
}
bool FramePresenter::submit(const PresentationFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (failure_)
        std::rethrow_exception(failure_);
    if (stopped_)
        return false;
    pending_ = frame;
    if (ready_)
        ++statistics_.replaced;
    ready_ = true;
    changed_.notify_one();
    return true;
}
void FramePresenter::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
        ready_ = false;
    }
    changed_.notify_one();
    if (worker_.joinable()) {
        // Present may need a synchronous message serviced by the UI thread.
        rxpce_ui::wait_for_thread(worker_.native_handle());
        worker_.join();
    }
}
PresentationStatistics FramePresenter::take_statistics() {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::exchange(statistics_, {});
}
void FramePresenter::run() noexcept {
    try {
        PresentationFrame current;
        for (;;) {
            {
                std::unique_lock<std::mutex> lock(mutex_);
                changed_.wait(lock, [&] { return stopped_ || ready_; });
                if (stopped_)
                    return;
                // Reuse both buffers. Producer never touches the in-flight image.
                std::swap(current, pending_);
                ready_ = false;
            }
            const auto begin = std::chrono::steady_clock::now();
            render_(current);
            const auto elapsed =
                static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                               std::chrono::steady_clock::now() - begin)
                                               .count());
            std::lock_guard<std::mutex> lock(mutex_);
            statistics_.present_us = (std::max)(statistics_.present_us, elapsed);
        }
    } catch (...) {
        std::lock_guard<std::mutex> lock(mutex_);
        failure_ = std::current_exception();
        stopped_ = true;
        ready_ = false;
    }
}
} // namespace rxpce_video
