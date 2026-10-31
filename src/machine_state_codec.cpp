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
    if (!rxpce_video::valid_vdc_mode(requirements.vdc_mode)) {
        error = "invalid VDC emulation mode";
        return false;
    }
    if (requirements.line_period <= 0 || requirements.timer_period <= 0 ||
        !requirements.cpu_clock_hz ||
        static_cast<std::uint64_t>(requirements.line_period) !=
            (std::uint64_t(requirements.cpu_clock_hz) * rxpce_video::vce_line_ticks +
             rxpce_video::video_master_hz - 1) /
                rxpce_video::video_master_hz) {
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
    if (value.scanline < -1 || value.scanline > 511) {
        error = "invalid state scanline";
        return false;
    }
    if (std::any_of(value.palette.begin(), value.palette.end(),
                    [](auto pixel) { return pixel >= 512; })) {
        error = "invalid state palette index";
        return false;
    }
    if (!valid_vdc(value.vdc, error))
        return false;
    const auto& t = value.vdc.scan;
    try {
        (void)encode_video_clock(value.video_clock);
        if (value.video_clock.cpu_hz != requirements.cpu_clock_hz)
            throw std::invalid_argument("Video clock setting mismatch");
        const auto& v = value.video_clock.vce;
        rxpce_video::CpuVideoClock conversion(requirements.cpu_clock_hz);
        conversion.restore(value.video_clock.cpu);
        rxpce_video::VceClock clock;
        clock.restore(v);
        if (t.dot_time != v.dots)
            throw std::invalid_argument("VCE/VDC dot clock mismatch");
        auto next_master = clock.next_event_time();
        rxpce_video::VdcScan scan;
        scan.restore(t);
        if (const auto dot = scan.next_event_dot()) {
            constexpr unsigned divs[] = {4, 3, 2, 2};
            const auto delta = (*dot - v.dots) * divs[v.control & 3] - v.dot_remainder;
            if (delta <= UINT64_MAX - v.master_time)
                next_master = (std::min)(next_master, v.master_time + delta);
        }
        const auto next_line = conversion.cpu_deadline(next_master);
        if (next_line <= value.cpu.cpu.elapsed ||
            next_line - value.cpu.cpu.elapsed != unsigned(value.cpu.line_remaining))
            throw std::invalid_argument("Invalid next video event deadline");
        const auto& a = t.active;
        const unsigned bg_width = (std::min)(128u, 32u << ((a.memory >> 4) & 3));
        if (value.io.screen_h != a.vdw + 1 || value.io.minline != 0 ||
            value.io.maxline != value.io.screen_h - 1 ||
            value.io.screen_w != (std::min)(512, int((a.hdr & 127) + 1) * 8) ||
            value.io.bg_w != int(bg_width) || value.io.bg_h != ((a.memory & 64) ? 64 : 32))
            throw std::invalid_argument("VDC geometry does not match period latches");
        const auto& q = t.requested;
        if (value.video_clock.cpu.cpu_time != value.cpu.cpu.elapsed || t.dot_time != v.dots ||
            t.physical_line != v.master_time / 1365 || value.scanline != t.row ||
            t.dot_time - t.last_line_dot > (v.line_master + 1) / 2 ||
            q.control != value.io.vdc[5].value || q.raster != value.io.vdc[6].value ||
            q.x_scroll != value.io.vdc[7].value || q.y_scroll != value.io.vdc[8].value ||
            q.memory != value.io.vdc[9].value || q.hsr != value.io.vdc[10].value ||
            q.hdr != value.io.vdc[11].value || q.vpr != value.io.vdc[12].value ||
            q.vdw != value.io.vdc[13].value || q.vcr != value.io.vdc[14].value)
            throw std::invalid_argument("VDC/VCE/CPU clock or register mismatch");
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
    if (requirements.vdc_mode == rxpce_video::VdcMode::fast &&
        (value.vdc.dma.phase != rxpce_video::DmaPhase::idle ||
         value.vdc.cpu.kind != rxpce_video::CpuVramKind::none || value.vdc.sprite_fetch_cells ||
         value.vdc.sprite_fetch_remaining)) {
        error = "invalid Fast VDC transfer";
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
    const auto vdc = encode_vdc(value.vdc);
    w.block(vdc.data(), vdc.size());
    const auto clock = encode_video_clock(value.video_clock);
    w.block(clock.data(), clock.size());
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
        std::array<char, vdc_state_bytes> vdc;
        r.block(vdc.data(), vdc.size());
        if (!decode_vdc(vdc.data(), vdc.size(), next->vdc, error))
            return false;
        std::array<char, video_clock_state_bytes> clock;
        r.block(clock.data(), clock.size());
        if (!decode_video_clock(clock.data(), clock.size(), requirements.cpu_clock_hz,
                                next->video_clock, error))
            return false;
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
