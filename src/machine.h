#pragma once
#include "vdc_mode.h"
#include "machine_memory.h"
#include "machine_io.h"
#include "graphics_memory.h"
#include "pce_cpu.h"
#include "video_frame.h"
#include "cartridge.h"
#include "cd_session.h"
#include "cd_controller.h"
#include "vdc_device.h"
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>

namespace rxpce_machine {
constexpr unsigned frame_width = 576, frame_height = 256;
struct VideoState {
    std::array<std::int16_t, 256> sprites{};
    std::array<rxpce_video::Pixel, 512> palette{};
    std::array<std::uint8_t, frame_width * frame_height> priority{};
    bool background_enabled = true, sprites_enabled = true, render_frame = true;
    int scroll_changed = 0, scroll_y_difference = 0, old_scroll_y_difference = 0;
    std::uint16_t old_scroll_x = 0, old_scroll_y = 0;
    int previous_line = 0, uses_sprite_priority = 0;
};
class Machine {
  public:
    struct Connections {
        std::uint8_t (*read_io)(void*, std::uint16_t) = nullptr;
        void (*write_io)(void*, std::uint16_t, std::uint8_t) = nullptr;
        void (*access)(void*, Region, std::uint32_t, bool) = nullptr;
        void (*bank_changed)(void*, std::uint8_t) = nullptr;
        void* context = nullptr;
    } connections;
    Machine() = default;
    ~Machine();
    Machine(const Machine&) = delete;
    Machine& operator=(const Machine&) = delete;
    Machine(Machine&&) = delete;
    Machine& operator=(Machine&&) = delete;

    Configuration configuration;
    rxpce_video::VdcMode vdc_mode = rxpce_video::VdcMode::accurate;
    std::array<std::uint8_t, main_ram_bytes> ram{};
    std::array<std::uint8_t, backup_ram_bytes> backup_ram{};
    std::array<std::uint8_t, 32768> populous{};
    std::array<std::uint8_t, 65536> cd_ram{};
    std::array<std::uint8_t, 196608> super_ram{};
    GraphicsMemory graphics;
    VideoState video;
    IoState io;
    PceCpu cpu;
    rxpce_video::VdcDevice vdc;
    int line_period = 1, timer_period = 1, frame_lines = 263, scanline = 0;
    bool frame_boundary_pending = false; // Runtime-only request, consumed at a CPU safe point.
    std::unique_ptr<rxpce_cd::Session> cd_session;
    rxpce_cd::Controller cd_controller;
    std::filesystem::path source, ram_path;
    bool backup_writable = false, cartridge_ram_writable = false;
    bool cd_execution_logged = false, cd_write_logged = false;
    unsigned bank_log_count = 0, input_read_log = 0;

    rxpce_cpu::Bus bus() noexcept;
    std::uint8_t read(std::uint32_t address);
    void write(std::uint32_t address, std::uint8_t value);
    void set_rom(std::vector<std::uint8_t> bytes, std::uint32_t flags);
    bool restore_bank(unsigned bank);
    void clear_media();
    const rxpce_rom::Mapper& mapper() const noexcept {
        return mapper_;
    }
    std::size_t rom_banks() const noexcept {
        return rom_.size() / 8192;
    }

  private:
    std::vector<std::uint8_t> rom_;
    rxpce_rom::Mapper mapper_;
    std::array<std::uint8_t*, 256> rom_map_{};
    void map_rom() noexcept;
};
} // namespace rxpce_machine
