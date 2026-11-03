#pragma once
#include "vdc_mode.h"
#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace rxpce_replay {
using Digest = std::array<std::uint8_t, 32>;
using Buttons = std::array<std::uint16_t, 5>;
enum class MediaKind : std::uint8_t { hucard, cue, chd };
struct Input {
    std::uint64_t time;
    Buttons buttons;
};
struct Checkpoint {
    std::uint32_t updates;
    std::uint64_t time;
    Digest digest;
};
struct Recording {
    MediaKind kind = MediaKind::hucard;
    rxpce_video::VdcMode mode = rxpce_video::VdcMode::accurate;
    std::uint32_t clock_hz = 7170000;
    std::uint8_t players = 1;
    std::array<std::uint8_t, 5> pad_modes{{2, 2, 2, 2, 2}};
    Digest media{}, bios{};
    std::vector<char> state;
    std::vector<Input> inputs;
    std::vector<Checkpoint> checkpoints;
};
inline constexpr std::size_t max_state_bytes = 4 * 1024 * 1024;
inline constexpr std::size_t max_inputs = 2160000;
inline constexpr std::size_t max_file_bytes = 64 * 1024 * 1024;
bool validate(const Recording& recording, std::string& error);
std::vector<char> encode(const Recording& recording);
bool decode(const std::vector<char>& bytes, Recording& recording, std::string& error);
bool load_file(const std::filesystem::path& path, Recording& recording, std::string& error);
bool save_file(const std::filesystem::path& path, const Recording& recording, bool replace,
               std::string& error);
} // namespace rxpce_replay
