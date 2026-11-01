#pragma once
#include <condition_variable>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace rxpce_log {
// The sink consumes the view synchronously on this writer's own thread.
class AsyncWriter {
  public:
    using Sink = std::function<void(std::string_view)>;
    using Clock = std::uint64_t (*)() noexcept;
    static std::uint64_t steady_milliseconds() noexcept {
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                              std::chrono::steady_clock::now().time_since_epoch())
                                              .count());
    }
    static constexpr std::size_t byte_limit = 1024 * 1024;
    explicit AsyncWriter(Sink sink, std::size_t bytes = byte_limit, std::size_t messages = 2048,
                         Clock clock = steady_milliseconds)
        : sink_(std::move(sink)), byte_limit_(bytes), message_limit_(messages), clock_(clock) {
        if (!sink_ || !bytes || !messages || !clock_)
            throw std::invalid_argument("Invalid log writer limits or sink");
        worker_ = std::thread([this] { run(); });
    }
    AsyncWriter(const AsyncWriter&) = delete;
    AsyncWriter& operator=(const AsyncWriter&) = delete;
    ~AsyncWriter() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        ready_.notify_one();
        worker_.join();
    }
    bool append(std::string text) {
        if (text.empty())
            return true;
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_)
            return false;
        if (faulted_ && clock_() - failed_at_ < 1000) {
            ++dropped_;
            return false;
        }
        if (text.size() > byte_limit_ - bytes_ ||
            pending_.size() + (writing_ ? 1 : 0) >= message_limit_) {
            ++dropped_;
            ready_.notify_one();
            return false;
        }
        const auto count = text.size();
        pending_.push_back(std::move(text));
        bytes_ += count;
        ready_.notify_one();
        return true;
    }
    void note_drop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!stopping_) {
            ++dropped_;
            if (faulted_ && clock_() - failed_at_ >= 1000)
                probe_ = true;
            ready_.notify_one();
        }
    }
    // For tests/shutdown boundaries; never call from the emulation or playback loop.
    void flush() {
        std::unique_lock<std::mutex> lock(mutex_);
        drained_.wait(lock, [this] {
            return pending_.empty() && !writing_ && !probe_ && (!dropped_ || faulted_);
        });
    }

  private:
    bool deliver(std::string_view text) noexcept {
        try {
            sink_(text);
            return true;
        } catch (...) {
            return false;
        }
    }
    void run() {
        for (;;) {
            std::string text;
            bool has_message = false;
            std::uint64_t dropped = 0;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                ready_.wait(lock, [this] {
                    return stopping_ || !pending_.empty() || probe_ || (!faulted_ && dropped_);
                });
                if (stopping_ && faulted_)
                    return;
                probe_ = false;
                if (pending_.empty() && !dropped_ && stopping_)
                    return;
                dropped = std::exchange(dropped_, 0);
                if (!pending_.empty()) {
                    text = std::move(pending_.front());
                    pending_.pop_front();
                    has_message = true;
                }
                writing_ = true;
            }
            bool summary_ok = true;
            if (dropped) {
                char warning[96];
                const int count =
                    std::snprintf(warning, sizeof(warning), "Log messages dropped: count=%llu\n",
                                  static_cast<unsigned long long>(dropped));
                if (count > 0 && static_cast<std::size_t>(count) < sizeof(warning))
                    summary_ok =
                        deliver(std::string_view(warning, static_cast<std::size_t>(count)));
            }
            const bool failed = !summary_ok || (has_message && !deliver(text));
            {
                std::lock_guard<std::mutex> lock(mutex_);
                bytes_ -= text.size();
                if (failed) {
                    if (!summary_ok)
                        dropped_ += dropped;
                    dropped_ += (has_message ? 1 : 0) + pending_.size();
                    pending_.clear();
                    bytes_ = 0;
                    faulted_ = true;
                    failed_at_ = clock_();
                    probe_ = false;
                } else {
                    faulted_ = false;
                }
                writing_ = false;
            }
            drained_.notify_all();
        }
    }
    Sink sink_;
    const std::size_t byte_limit_, message_limit_;
    Clock clock_;
    std::uint64_t failed_at_ = 0;
    bool faulted_ = false, probe_ = false;
    std::mutex mutex_;
    std::condition_variable ready_, drained_;
    std::deque<std::string> pending_;
    std::size_t bytes_ = 0;
    std::uint64_t dropped_ = 0;
    bool writing_ = false, stopping_ = false;
    std::thread worker_;
};
} // namespace rxpce_log
