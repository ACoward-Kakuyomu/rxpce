#pragma once
#include "machine_io.h"
#include "psg_state.h"

namespace rxpce_audio {
inline PsgState snapshot_psg(const rxpce_machine::IoState& io) {
    return {io.psg_volume, io.psg, io.wave};
}
} // namespace rxpce_audio
