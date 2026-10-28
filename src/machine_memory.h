#pragma once
#include <cstddef>
#include <cstdint>

namespace rxpce_machine {
constexpr std::size_t main_ram_bytes = 8192, backup_ram_bytes = 2048;
enum class Kind { hucard, cdrom, super_cdrom };
enum class Region { unmapped, cartridge, bios, main_ram, backup_ram, cd_ram, super_ram, io };
struct Location {
    Region region;
    std::uint32_t offset;
};
struct Configuration {
    Kind kind = Kind::hucard;
    bool japanese = true, cd_attached = true;
    std::uint8_t joy_signature() const {
        return static_cast<std::uint8_t>(0x30 | (japanese ? 0x40 : 0) | (cd_attached ? 0 : 0x80));
    }
    Location resolve(std::uint32_t address) const {
        if (address >= 0x200000)
            return {Region::unmapped, 0};
        const auto bank = address >> 13, offset = address & 8191;
        if (bank == 0xff)
            return {Region::io, offset};
        if (bank >= 0xf8 && bank <= 0xfb)
            return {Region::main_ram, offset};
        if (bank == 0xf7 && offset < backup_ram_bytes)
            return {Region::backup_ram, offset};
        if (bank >= 0xf7)
            return {Region::unmapped, 0};
        if (kind == Kind::hucard)
            return {Region::cartridge, address};
        if (address < 0x80000)
            return {Region::bios, address & 0x3ffff};
        if (kind == Kind::super_cdrom && address >= 0xd0000 && address < 0x100000)
            return {Region::super_ram, address - 0xd0000};
        if (address >= 0x100000 && address < 0x110000)
            return {Region::cd_ram, address - 0x100000};
        return {Region::unmapped, 0};
    }
};
struct BackupGate {
    std::uint8_t latch = 0, unlocked = 0;
    void reset() {
        latch = unlocked = 0;
    }
    void lock() {
        unlocked = 0;
    }
    void unlock(std::uint8_t value) {
        latch = value;
        if (value & 0x80)
            unlocked = 1;
    }
    std::uint8_t status() const {
        return (latch & 0x7f) | (unlocked ? 0x80 : 0);
    }
    bool valid() const {
        return unlocked <= 1;
    }
    std::uint8_t read(const unsigned char* ram, unsigned offset) const {
        return unlocked && offset < backup_ram_bytes ? ram[offset] : 0xff;
    }
    void write(unsigned char* ram, unsigned offset, std::uint8_t value) const {
        if (unlocked && offset < backup_ram_bytes)
            ram[offset] = value;
    }
};
} // namespace rxpce_machine
