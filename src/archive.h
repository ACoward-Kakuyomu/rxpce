#pragma once
#include "vdc_mode.h"
#include "cartridge.h"
#include <cstddef>
#include <string>
#include <filesystem>
#include <vector>
namespace rxpce_rom {
constexpr std::size_t max_rom_bytes = 8 * 1024 * 1024 + 512;
struct Cartridge {
    std::filesystem::path source_path;
    std::string member_name;
    std::vector<unsigned char> bytes;
    Profile profile;
};
bool load(const std::filesystem::path& path, Cartridge& result, std::string& error);
std::filesystem::path state_path(const std::filesystem::path& source,
                                 const std::filesystem::path& folder,
                                 rxpce_video::VdcMode mode = rxpce_video::VdcMode::accurate);
std::filesystem::path ram_path(const std::filesystem::path& source,
                               const std::filesystem::path& folder);
void queue(Cartridge image);
bool take(Cartridge& result);
bool has_pending();
} // namespace rxpce_rom
