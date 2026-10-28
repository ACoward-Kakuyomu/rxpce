#include "cartridge.h"
#include "nvram.h"
namespace rxpce_rom {
bool load_ram(const std::filesystem::path& path, unsigned char* ram, std::string& error) {
    return rxpce_storage::load_nvram(path.native(), ram, cartridge_ram_bytes, error);
}
bool save_ram(const std::filesystem::path& path, const unsigned char* ram, std::string& error) {
    return rxpce_storage::save_nvram(path.native(), ram, cartridge_ram_bytes, error);
}
} // namespace rxpce_rom
