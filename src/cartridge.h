#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <filesystem>
#include <vector>

namespace rxpce_rom {
constexpr std::size_t identity_prefix_bytes = 8192;
constexpr std::uint32_t populous_ram = 1, sf2_banking = 2, populous_ram_alias80 = 4;
constexpr std::size_t cartridge_ram_bytes = 32768;
bool load_ram(const std::filesystem::path& path, unsigned char* ram, std::string& error);
bool save_ram(const std::filesystem::path& path, const unsigned char* ram, std::string& error);
struct Identity {
    std::size_t rom_bytes = 0;
    std::string prefix_sha256;
};
struct Profile {
    Identity identity;
    std::uint32_t flags = 0;
    const char* name = "Standard HuCard";
};
std::string sha256(const unsigned char* bytes, std::size_t size);
Profile identify(const std::vector<unsigned char>& bytes);
Profile lookup(const Identity& identity);

class Mapper {
  public:
    static constexpr unsigned unmapped = ~0u;
    void configure(unsigned rom_banks, std::uint32_t flags);
    unsigned rom_bank(unsigned physical_bank) const;
    bool ram_bank(unsigned physical_bank) const;
    unsigned ram_offset(unsigned physical_bank) const;
    bool select(std::uint32_t physical_address);
    bool valid_selection(unsigned selection) const;
    void restore(unsigned selection);
    void reset() {
        selection_ = 0;
    }
    std::uint8_t selection() const {
        return selection_;
    }
    std::uint32_t flags() const {
        return flags_;
    }

  private:
    unsigned rom_banks_ = 0;
    std::uint32_t flags_ = 0;
    std::uint8_t selection_ = 0;
};
} // namespace rxpce_rom
