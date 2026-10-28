#define NOMINMAX
#include "cartridge.h"
#include <windows.h>
#include <bcrypt.h>
#include <limits>
#include <stdexcept>

namespace rxpce_rom {
std::string sha256(const unsigned char* bytes, std::size_t size) {
    if (size > std::numeric_limits<ULONG>::max() || (!bytes && size))
        throw std::runtime_error("Invalid SHA-256 input size");
    struct Algorithm {
        BCRYPT_ALG_HANDLE handle = nullptr;
        ~Algorithm() {
            if (handle)
                BCryptCloseAlgorithmProvider(handle, 0);
        }
    } algorithm;
    if (BCryptOpenAlgorithmProvider(&algorithm.handle, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        throw std::runtime_error("Cannot initialize ROM SHA-256");
    unsigned char digest[32]{}, empty = 0;
    auto input = const_cast<PUCHAR>(bytes ? bytes : &empty);
    if (BCryptHash(algorithm.handle, nullptr, 0, input, static_cast<ULONG>(size), digest,
                   sizeof(digest)) < 0)
        throw std::runtime_error("Cannot hash ROM prefix");
    const char hex[] = "0123456789abcdef";
    std::string result;
    for (auto byte : digest) {
        result += hex[byte >> 4];
        result += hex[byte & 15];
    }
    return result;
}
Profile lookup(const Identity& identity) {
    struct Entry {
        std::size_t bytes;
        const char* hash;
        std::uint32_t flags;
        const char* name;
    };
    static const Entry entries[] = {
        {524288, "63e3bbae4d8bb9dbb6b47ead73855a252fd1fe079773290fe5dd5501376e0463",
         populous_ram | populous_ram_alias80, "Populous"},
        {2621440, "7cb8123f3763cf59a44696a6427d9a985f3c96db35d929e0cefc7470c2f4bcb5", sf2_banking,
         "Street Fighter II'"}};
    Profile result;
    result.identity = identity;
    for (const auto& entry : entries)
        if (identity.rom_bytes == entry.bytes && identity.prefix_sha256 == entry.hash) {
            result.flags = entry.flags;
            result.name = entry.name;
            break;
        }
    return result;
}
Profile identify(const std::vector<unsigned char>& bytes) {
    if (bytes.size() < identity_prefix_bytes)
        throw std::runtime_error("ROM prefix is shorter than 8 KiB");
    return lookup({bytes.size(), sha256(bytes.data(), identity_prefix_bytes)});
}
void Mapper::configure(unsigned banks, std::uint32_t flags) {
    rom_banks_ = banks;
    flags_ = flags;
    reset();
}
unsigned Mapper::rom_bank(unsigned bank) const {
    if (bank >= 0xf7 || !rom_banks_)
        return unmapped;
    unsigned mapped;
    if (flags_ & sf2_banking) {
        if (bank >= 128)
            return unmapped;
        mapped = bank < 64 ? bank : bank + selection_ * 64;
    } else if (rom_banks_ == 48) {
        // Preserve the legacy 384 KiB layout while keeping every pointer in range.
        static const unsigned chunks[] = {0, 1, 0, 2, 2, 1, 0, 2};
        mapped = chunks[(bank >> 4) & 7] * 16 + (bank & 15);
    } else {
        unsigned mask = 1;
        while (mask < rom_banks_)
            mask <<= 1;
        mapped = bank & (mask - 1);
    }
    return mapped < rom_banks_ ? mapped : unmapped;
}
bool Mapper::ram_bank(unsigned bank) const {
    return (flags_ & populous_ram) &&
           ((bank >= 64 && bank <= 67) ||
            ((flags_ & populous_ram_alias80) && bank >= 128 && bank <= 131));
}
unsigned Mapper::ram_offset(unsigned bank) const {
    return ram_bank(bank) ? (bank & 3) * 8192 : unmapped;
}
bool Mapper::select(std::uint32_t address) {
    if (!(flags_ & sf2_banking) || address < 0x1ff0 || address > 0x1ff3)
        return false;
    selection_ = static_cast<std::uint8_t>(address & 3);
    return true;
}
bool Mapper::valid_selection(unsigned selection) const {
    return selection < ((flags_ & sf2_banking) ? 4u : 1u);
}
void Mapper::restore(unsigned selection) {
    if (!valid_selection(selection))
        throw std::runtime_error("Invalid cartridge bank state");
    selection_ = static_cast<std::uint8_t>(selection);
}
} // namespace rxpce_rom
