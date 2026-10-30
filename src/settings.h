#pragma once
#include "vdc_mode.h"
#include <array>
#include <atomic>
#include <string>
#include <filesystem>
#include "video_layout.h"
namespace rxpce_config {
struct Settings {
    struct Video {
        rxpce_video::AspectMode aspect_mode = rxpce_video::AspectMode::square_pixels;
        int adapter = -1, zoom = 1, frame_skip = 0;
        bool auto_frame_skip = true;
        std::string palette_file;
    } video;
    struct Audio {
        bool enabled = true;
        std::string backend = "wasapi";
        int sample_rate = 44100, channels = 2, latency_ms = 40, cooperative_level = 2;
    } audio;
    struct Player {
        std::string device = "keyboard";
        int buttons = 2;
        std::array<int, 14> keyboard{
            {'Z', 'X', 'A', 'S', 13, 9, 40, 38, 37, 39, 'C', 'A', 'S', 'D'}};
        std::array<int, 10> pad{{0, 1, 4, 5, 2, 3, 6, 7, 8, 9}};
        std::array<int, 10> xinput{{0, 1, 4, 5, 7, 6, 2, 3, 4, 5}};
    };
    struct Input {
        int count = 1;
        bool xinput_enabled = true, directinput_enabled = false;
        std::array<Player, 5> players;
    } input;
    struct Folders {
        std::string rom, state = ".";
    } folders;
    struct Cd {
        std::string bios_file;
    } cd;
    struct Advanced {
        rxpce_video::VdcMode vdc_emulation = rxpce_video::VdcMode::accurate;
        int base_clock_hz = 7170000;
        bool log = false, show_fps = false;
    } advanced;
};
struct RuntimeOverrides {
    bool auto_skip = false, skip = false, fps = false, rom = false;
};
void merge_runtime(Settings& pending, const Settings& runtime, const RuntimeOverrides& overrides);
bool decode(const std::string& json, Settings& settings, std::string& error);
std::string encode(const Settings& settings);
bool validate(const Settings& settings, std::string& error);
bool load(const std::filesystem::path& path, Settings& settings, std::string& error);
bool save(const std::filesystem::path& path, const Settings& settings, std::string& error);
std::filesystem::path config_path(const std::filesystem::path& executable);
std::wstring to_wide(const std::string& utf8);
std::string to_utf8(const std::wstring& wide);
std::string from_native(const std::string& native);
extern std::atomic_bool dialog_open;
void begin_dialog_input();
void end_dialog_input();
class DialogInputScope {
  public:
    // UI thread only; nested native modal loops inherit the outer suppression.
    DialogInputScope() : owns_suppression_(!dialog_open.load()) {
        if (owns_suppression_)
            begin_dialog_input();
    }
    ~DialogInputScope() {
        if (owns_suppression_)
            end_dialog_input();
    }
    DialogInputScope(const DialogInputScope&) = delete;
    DialogInputScope& operator=(const DialogInputScope&) = delete;

  private:
    bool owns_suppression_;
};
bool block_game_input(const unsigned char* keys, bool pad_button_down = false);
extern Settings::Input active_input;
} // namespace rxpce_config
