#include "memory_search.h"
#include "machine.h"

namespace rxpce_search {
Snapshot capture_ram(const rxpce_machine::Machine& machine) {
    Snapshot snapshot;
    const auto add = [&](RegionId id, const char* name, const auto& bytes) {
        snapshot.regions.push_back({name, {bytes.begin(), bytes.end()}, id});
    };
    add(RegionId::main_ram, "Main RAM", machine.ram);
    add(RegionId::backup_ram, "Backup RAM", machine.backup_ram);
    if (machine.configuration.kind != rxpce_machine::Kind::hucard)
        add(RegionId::cd_ram, "CD RAM", machine.cd_ram);
    if (machine.configuration.kind == rxpce_machine::Kind::super_cdrom)
        add(RegionId::super_ram, "Super CD RAM", machine.super_ram);
    if (machine.configuration.kind == rxpce_machine::Kind::hucard &&
        (machine.mapper().flags() & rxpce_rom::populous_ram))
        add(RegionId::hucard_ram, "HuCard RAM", machine.populous);
    return snapshot;
}
} // namespace rxpce_search
