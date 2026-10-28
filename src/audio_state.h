#pragma once
#include "audio_mixer.h"
#include "state_codec.h"
namespace rxpce_audio {
struct PsgPhaseState {
    unsigned rate = 44100;
    PsgSynth::State voices{};
};
constexpr std::size_t phase_state_bytes = 4 + 6 * 20;
inline bool valid_phase(const PsgPhaseState& state) {
    if (state.rate < 1000 || state.rate > 192000)
        return false;
    for (const auto& voice : state.voices)
        if (voice.index >= 32 || voice.noise > 1 || voice.phase >= 10ull * state.rate)
            return false;
    return true;
}
inline std::vector<char> encode_phase(const PsgPhaseState& state) {
    rxpce_state::Writer w;
    w.number(state.rate, 4);
    for (const auto& voice : state.voices) {
        w.number(voice.index, 4);
        w.number(voice.noise, 4);
        w.number(voice.seed, 4);
        w.number(voice.phase, 8);
    }
    return std::move(w.data);
}
inline bool decode_phase(const char* data, std::size_t size, PsgPhaseState& output) {
    if (!data || size != phase_state_bytes)
        return false;
    rxpce_state::Reader r(data, size);
    PsgPhaseState next;
    next.rate = static_cast<unsigned>(r.number(4));
    for (auto& voice : next.voices) {
        voice.index = static_cast<std::uint32_t>(r.number(4));
        voice.noise = static_cast<std::uint32_t>(r.number(4));
        voice.seed = static_cast<std::uint32_t>(r.number(4));
        voice.phase = r.number(8);
    }
    if (!valid_phase(next))
        return false;
    output = next;
    return true;
}
} // namespace rxpce_audio
