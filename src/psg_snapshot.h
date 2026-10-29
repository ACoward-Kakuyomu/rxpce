#pragma once
#include "machine_io.h"
#include "psg_state.h"

namespace rxpce_audio {
inline PsgState snapshot_psg(const rxpce_machine::IoState& io) {
    PsgState state;
    state.volume = io.psg_volume;
    state.registers = io.psg;
    state.wave = io.wave;
    state.lfo_frequency = io.psg_lfo_freq;
    state.lfo_control = io.psg_lfo_ctrl;
    return state;
}
} // namespace rxpce_audio
