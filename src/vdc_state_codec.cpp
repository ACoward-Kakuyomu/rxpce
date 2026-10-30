#include "vdc_state_codec.h"
#include "state_codec.h"
#include <stdexcept>
namespace rxpce_state {
bool valid_vdc(const VdcSnapshot& s, std::string& error) {
    try {
        rxpce_video::VdcTiming clock(s.config);
        clock.restore(s.timing);
        if (!rxpce_video::valid_dma(s.dma) || s.dma.dot_time != s.timing.dots)
            throw std::invalid_argument("Invalid VDC DMA phase or clock");
        if (static_cast<unsigned>(s.cpu.kind) > 2 || s.cpu.elapsed > 3 ||
            (s.cpu.kind == rxpce_video::CpuVramKind::none && s.cpu.elapsed) ||
            (s.cpu.increment != 1 && s.cpu.increment != 32 && s.cpu.increment != 64 &&
             s.cpu.increment != 128))
            throw std::invalid_argument("Invalid pending CPU VRAM access");
        error.clear();
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}
std::vector<char> encode_vdc(const VdcSnapshot& s) {
    std::string error;
    if (!valid_vdc(s, error))
        throw std::invalid_argument(error);
    Writer w;
    const auto& c = s.config;
    const auto& t = s.timing;
    const auto& d = s.dma;
    for (auto v : {c.line_period, c.frame_lines, c.first_active, c.last_active})
        w.number(v, 4);
    w.number(t.cpu_time, 8);
    w.number(t.dots, 8);
    w.number(t.line_offset, 4);
    w.number(t.line, 4);
    w.number(t.dot_remainder, 1);
    w.number(t.vce_control, 1);
    for (auto v : {t.requested_cr, t.line_cr, t.requested_mwr, t.active_mwr})
        w.number(v, 2);
    w.number(t.burst, 1);
    w.number(t.line_dots, 8);
    w.number(d.dot_time, 8);
    w.number(static_cast<unsigned>(d.phase), 1);
    w.number(d.source, 2);
    w.number(d.destination, 2);
    w.number(d.remaining, 4);
    w.number(d.direction, 1);
    w.number(d.read_latch, 2);
    w.number(d.elapsed, 1);
    w.number(d.satb_requested, 1);
    w.number(d.satb_running, 1);
    w.number(d.satb_request_source, 2);
    w.number(d.satb_source, 2);
    w.number(d.satb_index, 2);
    w.number(d.satb_elapsed, 1);
    w.number(static_cast<unsigned>(s.cpu.kind), 1);
    w.number(s.cpu.address, 2);
    w.number(s.cpu.value, 2);
    w.number(s.cpu.read_buffer, 2);
    w.number(s.cpu.elapsed, 1);
    w.number(s.cpu.increment, 2);
    if (w.data.size() != vdc_state_bytes)
        throw std::logic_error("VDC wire size mismatch");
    return w.data;
}
bool decode_vdc(const char* data, std::size_t size, VdcSnapshot& output, std::string& error) {
    if (!data || size != vdc_state_bytes) {
        error = "Invalid VDC wire length";
        return false;
    }
    try {
        Reader r(data, size);
        VdcSnapshot next;
        auto& c = next.config;
        auto& t = next.timing;
        auto& d = next.dma;
        c.line_period = static_cast<std::uint32_t>(r.number(4));
        c.frame_lines = static_cast<std::uint32_t>(r.number(4));
        c.first_active = static_cast<std::uint32_t>(r.number(4));
        c.last_active = static_cast<std::uint32_t>(r.number(4));
        t.cpu_time = r.number(8);
        t.dots = r.number(8);
        t.line_offset = static_cast<std::uint32_t>(r.number(4));
        t.line = static_cast<std::uint32_t>(r.number(4));
        t.dot_remainder = static_cast<std::uint8_t>(r.number(1));
        t.vce_control = static_cast<std::uint8_t>(r.number(1));
        t.requested_cr = static_cast<std::uint16_t>(r.number(2));
        t.line_cr = static_cast<std::uint16_t>(r.number(2));
        t.requested_mwr = static_cast<std::uint16_t>(r.number(2));
        t.active_mwr = static_cast<std::uint16_t>(r.number(2));
        t.burst = r.boolean();
        t.line_dots = r.number(8);
        d.dot_time = r.number(8);
        d.phase = static_cast<rxpce_video::DmaPhase>(r.number(1));
        d.source = static_cast<std::uint16_t>(r.number(2));
        d.destination = static_cast<std::uint16_t>(r.number(2));
        d.remaining = static_cast<std::uint32_t>(r.number(4));
        d.direction = static_cast<std::uint8_t>(r.number(1));
        d.read_latch = static_cast<std::uint16_t>(r.number(2));
        d.elapsed = static_cast<std::uint8_t>(r.number(1));
        d.satb_requested = r.boolean();
        d.satb_running = r.boolean();
        d.satb_request_source = static_cast<std::uint16_t>(r.number(2));
        d.satb_source = static_cast<std::uint16_t>(r.number(2));
        d.satb_index = static_cast<std::uint16_t>(r.number(2));
        d.satb_elapsed = static_cast<std::uint8_t>(r.number(1));
        next.cpu.kind = static_cast<rxpce_video::CpuVramKind>(r.number(1));
        next.cpu.address = static_cast<std::uint16_t>(r.number(2));
        next.cpu.value = static_cast<std::uint16_t>(r.number(2));
        next.cpu.read_buffer = static_cast<std::uint16_t>(r.number(2));
        next.cpu.elapsed = static_cast<std::uint8_t>(r.number(1));
        next.cpu.increment = static_cast<std::uint16_t>(r.number(2));
        if (!valid_vdc(next, error))
            return false;
        output = next;
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}
} // namespace rxpce_state
