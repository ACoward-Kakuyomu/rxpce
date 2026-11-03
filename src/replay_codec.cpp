#include "replay_codec.h"
#include "state_codec.h"
#include "state_format.h"
#include <algorithm>
#include <utility>

namespace rxpce_replay {
namespace {
constexpr char magic[] = "RXPCERP";
// a72e12d7-93a8-4859-a3ce-26764fd2be28
constexpr std::array<std::uint8_t, 16> format{
    0xa7, 0x2e, 0x12, 0xd7, 0x93, 0xa8, 0x48, 0x59,
    0xa3, 0xce, 0x26, 0x76, 0x4f, 0xd2, 0xbe, 0x28};
constexpr std::size_t header_bytes = 112, input_bytes = 18, checkpoint_bytes = 44;
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool valid_lengths(std::size_t state, std::size_t inputs, std::size_t checkpoints) {
    return state > rxpce_state::header_size && state <= max_state_bytes &&
           inputs && inputs <= max_inputs && checkpoints && checkpoints <= inputs &&
           header_bytes + state + inputs * input_bytes + checkpoints * checkpoint_bytes <=
               max_file_bytes;
}
} // namespace
bool validate(const Recording& r, std::string& error) {
    try {
        require(valid_lengths(r.state.size(), r.inputs.size(), r.checkpoints.size()),
                "Invalid replay section lengths");
        const bool cd = r.kind != MediaKind::hucard;
        require(!cd || r.kind == MediaKind::cue || r.kind == MediaKind::chd,
                "Unknown replay media kind");
        require(r.media != Digest{} && (cd == (r.bios != Digest{})),
                "Invalid replay media identity");
        require(rxpce_video::valid_vdc_mode(r.mode) && r.clock_hz >= 1000000 &&
                    r.clock_hz <= 85920000 && r.players >= 1 && r.players <= 5,
                "Invalid replay machine configuration");
        for (auto mode : r.pad_modes)
            require(mode == 2 || mode == 6, "Invalid replay controller mode");

        rxpce_state::Reader state(r.state.data() + 28, r.state.size() - 28);
        const auto flags = static_cast<std::uint32_t>(state.number(4));
        require(cd ? (flags == 0x100 || flags == 0x200) : (flags & ~7u) == 0,
                "Replay state does not match media kind");
        const auto prefix = rxpce_state::header(r.state.size() - rxpce_state::header_size,
                                               flags, r.mode);
        require(std::equal(prefix.begin(), prefix.end(), r.state.begin()),
                "Invalid or incompatible replay start state");

        for (std::size_t i = 0; i < r.inputs.size(); ++i) {
            require(i == 0 || r.inputs[i].time > r.inputs[i - 1].time,
                    "Replay input times are out of order");
            for (auto buttons : r.inputs[i].buttons)
                require(buttons <= 0xfff, "Invalid replay buttons");
        }
        std::uint32_t previous = 0;
        for (const auto& point : r.checkpoints) {
            require(point.updates > previous && point.updates <= r.inputs.size(),
                    "Invalid replay checkpoint index");
            const bool terminal = point.updates == r.inputs.size();
            require(terminal ? point.time > r.inputs.back().time
                             : point.time == r.inputs[point.updates].time,
                    "Invalid replay checkpoint time");
            previous = point.updates;
        }
        require(previous == r.inputs.size(), "Replay is missing its end checkpoint");
        error.clear();
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}
std::vector<char> encode(const Recording& r) {
    std::string error;
    if (!validate(r, error))
        throw std::invalid_argument(error);
    rxpce_state::Writer w;
    w.data.reserve(header_bytes + r.state.size() + r.inputs.size() * input_bytes +
                   r.checkpoints.size() * checkpoint_bytes);
    w.block(magic, sizeof(magic));
    w.block(format.data(), format.size());
    w.number(static_cast<std::uint8_t>(r.kind), 1);
    w.number(static_cast<std::uint8_t>(r.mode), 1);
    w.number(r.clock_hz, 4);
    w.number(r.players, 1);
    w.block(r.pad_modes.data(), r.pad_modes.size());
    w.block(r.media.data(), r.media.size());
    w.block(r.bios.data(), r.bios.size());
    w.number(r.state.size(), 4);
    w.number(r.inputs.size(), 4);
    w.number(r.checkpoints.size(), 4);
    w.block(r.state.data(), r.state.size());
    for (const auto& input : r.inputs) {
        w.number(input.time, 8);
        for (auto buttons : input.buttons)
            w.number(buttons, 2);
    }
    for (const auto& point : r.checkpoints) {
        w.number(point.updates, 4);
        w.number(point.time, 8);
        w.block(point.digest.data(), point.digest.size());
    }
    return std::move(w.data);
}
bool decode(const std::vector<char>& bytes, Recording& recording, std::string& error) {
    try {
        require(bytes.size() >= header_bytes && bytes.size() <= max_file_bytes,
                "Invalid replay file length");
        rxpce_state::Reader reader(bytes.data(), bytes.size());
        std::array<char, sizeof(magic)> signature{};
        auto guid = format;
        reader.block(signature.data(), signature.size());
        reader.block(guid.data(), guid.size());
        require(std::equal(signature.begin(), signature.end(), magic) && guid == format,
                "Unknown replay format");
        Recording next;
        next.kind = static_cast<MediaKind>(reader.number(1));
        next.mode = static_cast<rxpce_video::VdcMode>(reader.number(1));
        next.clock_hz = static_cast<std::uint32_t>(reader.number(4));
        next.players = static_cast<std::uint8_t>(reader.number(1));
        reader.block(next.pad_modes.data(), next.pad_modes.size());
        reader.block(next.media.data(), next.media.size());
        reader.block(next.bios.data(), next.bios.size());
        const auto state = static_cast<std::size_t>(reader.number(4));
        const auto inputs = static_cast<std::size_t>(reader.number(4));
        const auto checkpoints = static_cast<std::size_t>(reader.number(4));
        require(valid_lengths(state, inputs, checkpoints) &&
                    reader.remaining == state + inputs * input_bytes + checkpoints * checkpoint_bytes,
                "Invalid replay section lengths");
        next.state.resize(state);
        reader.block(next.state.data(), state);
        next.inputs.resize(inputs);
        for (auto& input : next.inputs) {
            input.time = reader.number(8);
            for (auto& buttons : input.buttons)
                buttons = static_cast<std::uint16_t>(reader.number(2));
        }
        next.checkpoints.resize(checkpoints);
        for (auto& point : next.checkpoints) {
            point.updates = static_cast<std::uint32_t>(reader.number(4));
            point.time = reader.number(8);
            reader.block(point.digest.data(), point.digest.size());
        }
        if (!validate(next, error))
            return false;
        recording = std::move(next);
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}
} // namespace rxpce_replay
