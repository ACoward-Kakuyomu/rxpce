#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <utility>

namespace rxpce_machine {
class GraphicsMemory {
  public:
    using CacheWord = std::uint32_t;
    static constexpr std::size_t vram_bytes = 0x20000;
    static constexpr std::size_t cache_words = vram_bytes / sizeof(CacheWord);
    static constexpr std::size_t background_tiles = vram_bytes / 32;
    static constexpr std::size_t sprite_tiles = vram_bytes / 128;

    bool initialize() noexcept {
        // Check allocation before touching any array; avoid a large temporary.
        std::unique_ptr<Storage> next(new (std::nothrow) Storage);
        if (!next)
            return false;
        next->vram.fill(0);
        next->background.fill(0);
        next->sprites.fill(0);
        next->background_dirty.fill(1);
        next->sprite_dirty.fill(1);
        storage_ = std::move(next);
        return true;
    }
    void reset() noexcept {
        storage_.reset();
    }
    std::uint8_t* vram() noexcept {
        return storage_ ? storage_->vram.data() : nullptr;
    }
    CacheWord* background() noexcept {
        return storage_ ? storage_->background.data() : nullptr;
    }
    CacheWord* sprites() noexcept {
        return storage_ ? storage_->sprites.data() : nullptr;
    }
    std::uint8_t* background_dirty() noexcept {
        return storage_ ? storage_->background_dirty.data() : nullptr;
    }
    std::uint8_t* sprite_dirty() noexcept {
        return storage_ ? storage_->sprite_dirty.data() : nullptr;
    }

  private:
    struct Storage {
        std::array<std::uint8_t, vram_bytes> vram;
        std::array<CacheWord, cache_words> background, sprites;
        std::array<std::uint8_t, background_tiles> background_dirty;
        std::array<std::uint8_t, sprite_tiles> sprite_dirty;
    };
    std::unique_ptr<Storage> storage_;
};
} // namespace rxpce_machine
