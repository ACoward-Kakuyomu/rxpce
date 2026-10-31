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
    state.selected = io.psg_ch;
    return state;
}
inline void apply_psg_registers(rxpce_machine::IoState& io, const PsgState& state) {
    io.psg_volume = state.volume;
    io.psg = state.registers;
    io.wave = state.wave;
    io.psg_lfo_freq = state.lfo_frequency;
    io.psg_lfo_ctrl = state.lfo_control;
    io.psg_ch = state.selected;
    // wavofs remains the legacy readback shadow, not the sound chip's write address.
}
} // namespace rxpce_audio
