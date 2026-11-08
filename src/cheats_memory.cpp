#include "cheats.h"
#include "machine.h"
#include "cheats_memory.h"

namespace rxpce_cheats {
detail::Ram detail::resolve_ram(rxpce_machine::Machine& machine, rxpce_search::RegionId region) {
    switch (region) {
    case rxpce_search::RegionId::main_ram:
        return {machine.ram.data(), machine.ram.size()};
    case rxpce_search::RegionId::cd_ram:
        if (machine.configuration.kind != rxpce_machine::Kind::hucard) {
            return {machine.cd_ram.data(), machine.cd_ram.size()};
        }
        break;
    case rxpce_search::RegionId::super_ram:
        if (machine.configuration.kind == rxpce_machine::Kind::super_cdrom) {
            return {machine.super_ram.data(), machine.super_ram.size()};
        }
        break;
    default: break;
    }
    return {};
}
bool apply_once(rxpce_machine::Machine& machine, const Entry& entry, std::string& error) {
    if (!entry.enabled || entry.always_apply) {
        error = "Select an enabled single-use cheat";
        return false;
    }
    const auto region = detail::resolve_ram(machine, entry.region);
    const auto data = region.data;
    const auto size = region.size;
    if (!data || (entry.bytes != 1 && entry.bytes != 2 && entry.bytes != 4) ||
        entry.offset > size || entry.bytes > size - entry.offset) {
        error = "Cheat target is unavailable, read-only or outside RAM";
        return false;
    }
    if (entry.value > (UINT32_MAX >> ((4 - entry.bytes) * 8))) {
        error = "Cheat value does not fit the selected size";
        return false;
    }
    for (unsigned i = 0; i < entry.bytes; ++i)
        data[entry.offset + i] = static_cast<std::uint8_t>(entry.value >> (i * 8));
    error.clear();
    return true;
}
} // namespace rxpce_cheats
