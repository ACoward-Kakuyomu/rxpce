#pragma once
#include "audio_resampler.h"
#include "state_codec.h"

namespace rxpce_audio {
constexpr std::size_t filter_tail_limit = 512;
constexpr std::size_t filter_state_bytes = 32 + filter_tail_limit * 16;
inline constexpr std::array<char, filter_tail_limit * 16> filter_padding{};
inline bool valid_filter_state(const BandlimitedBuffer::State& state) {
    if (!state.rate)
        return !state.pcm_rate && !state.cursor && state.tail.empty() &&
               state.integral == std::array<double, 2>{};
    BandlimitedBuffer probe;
    return probe.restore(state);
}
inline void write_filter_state(rxpce_state::Writer& writer, const BandlimitedBuffer::State& state) {
    if (state.tail.size() > filter_tail_limit)
        throw std::invalid_argument("Audio filter tail is too large");
    writer.data.reserve(writer.data.size() + filter_state_bytes);
    writer.number(state.rate, 4);
    writer.number(state.pcm_rate, 4);
    writer.number(state.cursor, 4);
    writer.number(state.tail.size(), 4);
    for (double value : state.integral)
        writer.real(value);
    for (const auto& pair : state.tail)
        for (double value : pair)
            writer.real(value);
    writer.block(filter_padding.data(), (filter_tail_limit - state.tail.size()) * 16);
}
inline bool read_filter_state(rxpce_state::Reader& reader, BandlimitedBuffer::State& output) {
    if (reader.remaining < filter_state_bytes)
        return false;
    BandlimitedBuffer::State state;
    state.rate = static_cast<unsigned>(reader.number(4));
    state.pcm_rate = static_cast<unsigned>(reader.number(4));
    state.cursor = static_cast<unsigned>(reader.number(4));
    const auto count = reader.number(4);
    if (count > filter_tail_limit)
        return false;
    state.tail.resize(static_cast<std::size_t>(count));
    for (auto& value : state.integral)
        value = reader.real();
    for (auto& pair : state.tail)
        for (auto& value : pair)
            value = reader.real();
    const auto padding = (filter_tail_limit - state.tail.size()) * 16;
    if (std::memcmp(reader.cursor, filter_padding.data(), padding) != 0 ||
        !valid_filter_state(state))
        return false;
    reader.cursor += padding;
    reader.remaining -= padding;
    output = std::move(state);
    return true;
}
} // namespace rxpce_audio
