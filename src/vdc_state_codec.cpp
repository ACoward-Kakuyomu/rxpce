#include "vdc_state_codec.h"
#include "state_codec.h"
#include <stdexcept>
#include <array>
namespace rxpce_state {
bool valid_vdc(const VdcSnapshot& s, std::string& error) {
    try {
        rxpce_video::VdcScan scan;
        scan.restore(s.scan);
        if (!rxpce_video::valid_dma(s.dma) || s.dma.dot_time != s.scan.dot_time)
            throw std::invalid_argument("Invalid VDC DMA phase or clock");
        if (s.sprite_fetch_cells > 16 || s.sprite_fetch_remaining > 256 || s.fast_satb_lines > 4)
            throw std::invalid_argument("Invalid sprite fetch reservation");
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
    const auto& d = s.dma;
    const auto scan = encode_vdc_scan(s.scan);
    w.block(scan.data(), scan.size());
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
    w.number(s.sprite_fetch_cells, 1);
    w.number(s.sprite_fetch_remaining, 2);
    w.number(s.fast_satb_lines, 1);
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
        auto& d = next.dma;
        std::array<char, vdc_scan_state_bytes> scan;
        r.block(scan.data(), scan.size());
        if (!decode_vdc_scan(scan.data(), scan.size(), next.scan, error))
            return false;
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
        next.sprite_fetch_cells = static_cast<std::uint8_t>(r.number(1));
        next.sprite_fetch_remaining = static_cast<std::uint16_t>(r.number(2));
        next.fast_satb_lines = static_cast<std::uint8_t>(r.number(1));
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
