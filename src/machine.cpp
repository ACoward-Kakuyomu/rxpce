#include "machine.h"
#include <stdexcept>
#include <utility>

namespace rxpce_machine {
Machine::~Machine() = default;
rxpce_cpu::Bus Machine::bus() noexcept {
    return {[](std::uint32_t address, void* context) {
                return static_cast<Machine*>(context)->read(address);
            },
            [](std::uint32_t address, std::uint8_t value, void* context) {
                static_cast<Machine*>(context)->write(address, value);
            },
            this,
            [](std::uint32_t address, bool, void*) -> unsigned {
                return (address & 0x1ff800) == 0x1fe000 ? 1 : 0;
            }};
}
void Machine::map_rom() noexcept {
    rom_map_.fill(nullptr);
    for (unsigned bank = 0; bank < 0xf7; ++bank) {
        const auto mapped = mapper_.rom_bank(bank);
        if (mapped < rom_banks())
            rom_map_[bank] = rom_.data() + mapped * 8192;
        if (mapper_.ram_bank(bank))
            rom_map_[bank] = populous.data() + mapper_.ram_offset(bank);
    }
}
void Machine::set_rom(std::vector<std::uint8_t> bytes, std::uint32_t flags) {
    if (bytes.size() < 8192 || bytes.size() % 8192 || bytes.size() > 8 * 1024 * 1024)
        throw std::invalid_argument("Invalid machine ROM length");
    rom_ = std::move(bytes);
    mapper_.configure(static_cast<unsigned>(rom_banks()), flags);
    map_rom();
}
bool Machine::restore_bank(unsigned bank) {
    if (!mapper_.valid_selection(bank))
        return false;
    mapper_.restore(bank);
    map_rom();
    return true;
}
void Machine::clear_media() {
    rom_map_.fill(nullptr);
    cd_controller.connect(nullptr, {});
    cd_session.reset();
    std::vector<std::uint8_t>().swap(rom_);
    mapper_.configure(0, 0);
    configuration.kind = Kind::hucard;
}
std::uint8_t Machine::read(std::uint32_t address) {
    const auto location = configuration.resolve(address);
    if ((location.region == Region::cd_ram || location.region == Region::super_ram) &&
        connections.access)
        connections.access(connections.context, location.region, address, false);
    switch (location.region) {
    case Region::io:
        return connections.read_io
                   ? connections.read_io(connections.context,
                                         static_cast<std::uint16_t>(location.offset))
                   : 0xff;
    case Region::bios:
        return location.offset < rom_.size() ? rom_[location.offset] : 0xff;
    case Region::cd_ram:
        return cd_ram[location.offset];
    case Region::super_ram:
        return super_ram[location.offset];
    case Region::main_ram:
        return ram[location.offset];
    case Region::backup_ram:
        return io.backup.read(backup_ram.data(), location.offset);
    case Region::cartridge:
        return rom_map_[address >> 13] ? rom_map_[address >> 13][address & 8191] : 0xff;
    default:
        return 0xff;
    }
}
void Machine::write(std::uint32_t address, std::uint8_t value) {
    const auto location = configuration.resolve(address);
    if ((location.region == Region::cd_ram || location.region == Region::super_ram) &&
        connections.access)
        connections.access(connections.context, location.region, address, true);
    switch (location.region) {
    case Region::io:
        if (connections.write_io)
            connections.write_io(connections.context, static_cast<std::uint16_t>(location.offset),
                                 value);
        return;
    case Region::cd_ram:
        cd_ram[location.offset] = value;
        return;
    case Region::super_ram:
        super_ram[location.offset] = value;
        return;
    case Region::main_ram:
        ram[location.offset] = value;
        return;
    case Region::backup_ram:
        io.backup.write(backup_ram.data(), location.offset, value);
        return;
    case Region::cartridge:
        break;
    default:
        return;
    }
    const auto previous = mapper_.selection();
    if (mapper_.select(address)) {
        map_rom();
        if (previous != mapper_.selection() && connections.bank_changed)
            connections.bank_changed(connections.context, mapper_.selection());
        return;
    }
    const unsigned bank = address >> 13;
    if (mapper_.ram_bank(bank) && rom_map_[bank])
        rom_map_[bank][address & 8191] = value;
}
} // namespace rxpce_machine
