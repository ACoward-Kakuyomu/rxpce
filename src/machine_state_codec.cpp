#include "machine_state_codec.h"
#include "state_codec.h"
#include "cartridge.h"
#include <algorithm>
#include <utility>

namespace rxpce_state {
namespace {
bool valid_requirements(const SnapshotRequirements& requirements, std::string& error) {
    constexpr auto known =
        rxpce_rom::populous_ram | rxpce_rom::sf2_banking | rxpce_rom::populous_ram_alias80;
    const bool cartridge = (requirements.flags & ~known) == 0 &&
                           (!(requirements.flags & rxpce_rom::populous_ram_alias80) ||
                            (requirements.flags & rxpce_rom::populous_ram));
    if (!cartridge && requirements.flags != 0x100 && requirements.flags != 0x200) {
        error = "unknown machine state flags";
        return false;
    }
    if (requirements.line_period <= 0 || requirements.timer_period <= 0 ||
        !requirements.frame_lines) {
        error = "invalid snapshot clocks";
        return false;
    }
    if (requirements.controllers < 1 || requirements.controllers > 5 ||
        std::any_of(requirements.buttons.begin(), requirements.buttons.end(),
                    [](auto v) { return v != 2 && v != 6; })) {
        error = "invalid input configuration";
        return false;
    }
    return true;
}
} // namespace
bool valid_machine(const MachineSnapshot& value, const SnapshotRequirements& requirements,
                   std::string& error) {
    if (!valid_requirements(requirements, error) || !valid_io(value.io, error))
        return false;
    if (value.scanline < 0 || static_cast<unsigned>(value.scanline) >= requirements.frame_lines) {
        error = "invalid state scanline";
        return false;
    }
    if (std::any_of(value.palette.begin(), value.palette.end(),
                    [](auto pixel) { return pixel >= 512; })) {
        error = "invalid state palette index";
        return false;
    }
    if (value.io.controllers.count != requirements.controllers ||
        value.io.controllers.modes != requirements.buttons) {
        error = "controller configuration mismatch";
        return false;
    }
    if ((requirements.flags & 2) ? value.bank >= 4 : value.bank != 0) {
        error = "invalid cartridge bank";
        return false;
    }
    const auto bytes = encode_cpu(value.cpu);
    PceCpu checked;
    if (!decode_cpu(bytes.data(), bytes.size(), requirements.line_period, requirements.timer_period,
                    checked)) {
        error = "invalid CPU state or clock mismatch";
        return false;
    }
    error.clear();
    return true;
}
std::vector<char> encode_machine(const MachineSnapshot& value,
                                 const SnapshotRequirements& requirements) {
    std::string error;
    if (!valid_machine(value, requirements, error))
        throw std::invalid_argument(error);
    Writer w;
    w.data.reserve(machine_state_bytes(requirements.flags));
    w.block(value.ram.data(), value.ram.size());
    w.block(value.vram.data(), value.vram.size());
    for (auto word : value.sprites)
        w.number(static_cast<std::uint16_t>(word), 2);
    for (auto pixel : value.palette)
        w.number(pixel, 2);
    w.number(static_cast<std::uint32_t>(value.scanline), 4);
    const auto io = encode_io(value.io);
    w.block(io.data(), io.size());
    w.block(value.backup.data(), value.backup.size());
    w.number(value.bank, 1);
    const auto cpu = encode_cpu(value.cpu);
    w.block(cpu.data(), cpu.size());
    if (requirements.flags & 1)
        w.block(value.populous.data(), value.populous.size());
    if (w.data.size() != machine_state_bytes(requirements.flags))
        throw std::logic_error("machine wire size mismatch");
    return std::move(w.data);
}
bool decode_machine(const char* data, std::size_t size, const SnapshotRequirements& requirements,
                    std::unique_ptr<MachineSnapshot>& output, std::string& error) {
    if (!valid_requirements(requirements, error))
        return false;
    if (!data || size != machine_state_bytes(requirements.flags)) {
        error = "invalid machine state length";
        return false;
    }
    try {
        Reader r(data, size);
        auto next = std::make_unique<MachineSnapshot>();
        r.block(next->ram.data(), next->ram.size());
        r.block(next->vram.data(), next->vram.size());
        for (auto& word : next->sprites)
            word = static_cast<std::int16_t>(r.signed_number(2));
        for (auto& pixel : next->palette)
            pixel = static_cast<rxpce_video::Pixel>(r.number(2));
        next->scanline = static_cast<std::int32_t>(r.signed_number(4));
        std::array<char, io_wire::size> io;
        r.block(io.data(), io.size());
        if (!decode_io(io.data(), io.size(), next->io, error))
            return false;
        r.block(next->backup.data(), next->backup.size());
        next->bank = static_cast<std::uint8_t>(r.number(1));
        std::array<char, cpu_state_size> cpu;
        r.block(cpu.data(), cpu.size());
        if (!decode_cpu(cpu.data(), cpu.size(), requirements.line_period, requirements.timer_period,
                        next->cpu)) {
            error = "invalid CPU state or clock mismatch";
            return false;
        }
        if (requirements.flags & 1)
            r.block(next->populous.data(), next->populous.size());
        if (r.remaining) {
            error = "trailing machine state";
            return false;
        }
        if (!valid_machine(*next, requirements, error))
            return false;
        output = std::move(next);
        error.clear();
        return true;
    } catch (const std::exception& failure) {
        error = failure.what();
        return false;
    }
}
std::vector<char> encode_hucard(const MachineSnapshot& value, const SnapshotRequirements& req) {
    if (!rxpce_audio::valid_phase(value.sound))
        throw std::invalid_argument("invalid PSG phase state");
    auto bytes = encode_machine(value, req);
    const auto phase = rxpce_audio::encode_phase(value.sound);
    bytes.insert(bytes.end(), phase.begin(), phase.end());
    return bytes;
}
bool decode_hucard(const char* data, std::size_t size, const SnapshotRequirements& req,
                   std::unique_ptr<MachineSnapshot>& output, std::string& error) {
    if (!data || size != hucard_state_bytes(req.flags)) {
        error = "invalid HuCard state length";
        return false;
    }
    std::unique_ptr<MachineSnapshot> next;
    const auto base = machine_state_bytes(req.flags);
    if (!decode_machine(data, base, req, next, error))
        return false;
    if (!rxpce_audio::decode_phase(data + base, size - base, next->sound)) {
        error = "invalid PSG phase state";
        return false;
    }
    output = std::move(next);
    return true;
}
} // namespace rxpce_state
