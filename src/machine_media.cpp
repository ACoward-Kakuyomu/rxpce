#include "replay_media.h"
#include "machine.h"

namespace rxpce_replay {
MediaIdentity current_media(const rxpce_machine::Machine& machine) {
    if (machine.cd_session) {
        const auto& cd = *machine.cd_session;
        return identify_cd(cd.source, cd.disc.identity(), cd.bios.profile.rom_sha256);
    }
    const auto& rom = machine.rom_bytes();
    return identify_hucard(rom.data(), rom.size());
}
}
