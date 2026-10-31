#pragma once
#include "audio_mixer.h"
#include "state_codec.h"
#include "audio_render_state.h"
namespace rxpce_audio {
struct PsgPhaseState {
    unsigned rate = 44100;
    PsgSynth::State voices{};
    unsigned base_clock = 7170000;
    std::uint32_t remainder = 0;
    unsigned channels = 2;
    BandlimitedBuffer::State render;
};
constexpr std::size_t phase_state_bytes = 12 + 6 * 16 + 4 + filter_state_bytes;
inline bool valid_phase(const PsgPhaseState& state) {
    if (state.rate < 1000 || state.rate > 192000 || !state.base_clock ||
        state.remainder >= state.base_clock || (state.channels != 1 && state.channels != 2) ||
        (state.render.rate && (state.render.rate != state.rate || state.render.pcm_rate)) ||
        !valid_filter_state(state.render))
        return false;
    for (unsigned ch = 0; ch < 6; ++ch) {
        const auto& voice = state.voices[ch];
        if (voice.index >= 32 || !voice.seed || voice.seed > 0x3ffff ||
            voice.tone_remaining > (ch == 1 ? 2097152u : 8192u) ||
            voice.noise_remaining > (ch >= 4 ? 3968u : 0u))
            return false;
    }
    return true;
}
inline std::vector<char> encode_phase(const PsgPhaseState& state) {
    rxpce_state::Writer w;
    w.number(state.rate, 4);
    w.number(state.base_clock, 4);
    w.number(state.remainder, 4);
    for (const auto& voice : state.voices) {
        w.number(voice.index, 4);
        w.number(voice.seed, 4);
        w.number(voice.tone_remaining, 4);
        w.number(voice.noise_remaining, 4);
    }
    w.number(state.channels, 4);
    write_filter_state(w, state.render);
    return std::move(w.data);
}
inline bool decode_phase(const char* data, std::size_t size, PsgPhaseState& output) {
    if (!data || size != phase_state_bytes)
        return false;
    rxpce_state::Reader r(data, size);
    PsgPhaseState next;
    next.rate = static_cast<unsigned>(r.number(4));
    next.base_clock = static_cast<unsigned>(r.number(4));
    next.remainder = static_cast<std::uint32_t>(r.number(4));
    for (auto& voice : next.voices) {
        voice.index = static_cast<std::uint32_t>(r.number(4));
        voice.seed = static_cast<std::uint32_t>(r.number(4));
        voice.tone_remaining = static_cast<std::uint32_t>(r.number(4));
        voice.noise_remaining = static_cast<std::uint32_t>(r.number(4));
    }
    next.channels = static_cast<unsigned>(r.number(4));
    if (!read_filter_state(r, next.render))
        return false;
    if (!valid_phase(next))
        return false;
    output = next;
    return true;
}
} // namespace rxpce_audio
