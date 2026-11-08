#pragma once
#include "cheats.h"
#include <condition_variable>
#include <functional>
#include <mutex>
#include <utility>

namespace rxpce_search {
// The worker owns live RAM. Requests and immutable copies cross this channel.
class Pause {
  public:
    struct Context {
        Snapshot snapshot;
        rxpce_cheats::Document document;
        bool overwrite_confirmation = false;
        std::string warning;
        std::uint64_t token = 0;
    };
    struct Request {
        enum class Kind { apply, save } kind = Kind::apply;
        std::uint64_t token = 0;
        rxpce_cheats::Entry entry;
        rxpce_cheats::Document document;
    };
    struct Reply {
        std::optional<Snapshot> snapshot;
        std::optional<rxpce_cheats::Document> saved;
        std::string error;
        std::uint64_t token = 0;
    };
    using Handler = std::function<Reply(const Request&)>;
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        cancelled_ = false;
        paused_ = false;
        clear_pending();
        ++epoch_;
    }
    void invalidate() {
        std::lock_guard<std::mutex> lock(mutex_);
        ++epoch_;
    }
    void cancel() {
        std::lock_guard<std::mutex> lock(mutex_);
        cancelled_ = true;
        paused_ = false;
        clear_pending();
        changed_.notify_all();
    }
    void resume() {
        std::lock_guard<std::mutex> lock(mutex_);
        paused_ = false;
        clear_pending();
        changed_.notify_all();
    }
    std::optional<Context> take() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto snapshot = std::move(context_);
        context_.reset();
        return snapshot;
    }
    bool request(Request request) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!paused_ || cancelled_ || request.token != token_ || request_ || busy_ || reply_)
            return false;
        request_ = std::move(request);
        changed_.notify_all();
        return true;
    }
    std::optional<Reply> take_reply() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto reply = std::move(reply_);
        reply_.reset();
        return reply;
    }
    void wait(Context context, const std::function<bool()>& notify,
              const Handler& handle = {}, const std::function<bool()>& notify_reply = {}) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (cancelled_)
            return;
        context.snapshot.epoch = epoch_;
        context.token = ++token_;
        context_ = std::move(context);
        paused_ = true;
        lock.unlock();
        if (!notify()) {
            resume();
            return;
        }
        lock.lock();
        while (paused_ && !cancelled_) {
            changed_.wait(lock, [&] { return !paused_ || cancelled_ || request_.has_value(); });
            if (!paused_ || cancelled_) break;
            auto request = std::move(*request_);
            request_.reset();
            busy_ = true;
            lock.unlock();
            Reply reply;
            try {
                if (handle) reply = handle(request);
                else reply.error = "Memory operations are not available";
            } catch (const std::exception& error) {
                reply.error = error.what();
            }
            reply.token = request.token;
            lock.lock();
            busy_ = false;
            if (!paused_ || cancelled_) break;
            if (reply.snapshot) reply.snapshot->epoch = epoch_;
            reply_ = std::move(reply);
            lock.unlock();
            if (!notify_reply || !notify_reply()) resume();
            lock.lock();
        }
    }
  private:
    void clear_pending() {
        context_.reset();
        request_.reset();
        reply_.reset();
    }
    std::mutex mutex_;
    std::condition_variable changed_;
    std::optional<Context> context_;
    std::optional<Request> request_;
    std::optional<Reply> reply_;
    std::uint64_t epoch_ = 0, token_ = 0;
    bool paused_ = false, cancelled_ = false, busy_ = false;
};
inline Pause& pause_channel() {
    static Pause channel;
    return channel;
}
} // namespace rxpce_search
