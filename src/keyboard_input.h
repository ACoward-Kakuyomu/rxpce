#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <mutex>
namespace rxpce_input {
class KeyboardState {
    mutable std::mutex mutex;
    std::array<unsigned char, 256> keys{};

  public:
    void reset() {
        std::lock_guard<std::mutex> lock(mutex);
        keys.fill(0);
    }
    void event(unsigned message, unsigned key, std::uintptr_t flags) {
        if (key >= keys.size())
            return;
        if (message != 0x100 && message != 0x101 && message != 0x104 && message != 0x105)
            return;
        std::lock_guard<std::mutex> lock(mutex);
        if (message == 0x101 || message == 0x105)
            keys[key] = 0;
        else if (!(flags & (std::uintptr_t(1) << 30)) || keys[key])
            keys[key] = 0x80;
    }
    void snapshot(unsigned char* target) const {
        std::lock_guard<std::mutex> lock(mutex);
        std::copy(keys.begin(), keys.end(), target);
    }
};
inline KeyboardState& keyboard_state() {
    static KeyboardState state;
    return state;
}
} // namespace rxpce_input
