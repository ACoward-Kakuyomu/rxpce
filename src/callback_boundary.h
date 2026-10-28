#pragma once
#include <exception>
#include <utility>

namespace rxpce_host {
// Synchronous native enumeration callback boundary. Inspect/rethrow only after
// the native API returns; no later callback may use partial results on failure.
class CallbackFailure {
  public:
    template <class Function> bool invoke(Function&& function) noexcept {
        if (failure_)
            return false;
        try {
            std::forward<Function>(function)();
            return true;
        } catch (...) {
            failure_ = std::current_exception();
            return false;
        }
    }
    bool failed() const noexcept {
        return static_cast<bool>(failure_);
    }
    void clear() noexcept {
        failure_ = nullptr;
    }
    void rethrow() const {
        if (failure_)
            std::rethrow_exception(failure_);
    }

  private:
    std::exception_ptr failure_;
};
} // namespace rxpce_host
