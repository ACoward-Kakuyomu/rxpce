#include "vdc_scan_state.h"
#include "state_codec.h"
namespace rxpce_state {
std::vector<char> encode_vdc_scan(const rxpce_video::VdcScanState& s) {
    rxpce_video::VdcScan checked;
    checked.restore(s);
    const auto& r = s.requested;
    const auto& a = s.active;
    Writer w;
    w.data.reserve(vdc_scan_state_bytes);
    for (auto v :
         {r.control, r.raster, r.x_scroll, r.y_scroll, r.memory, r.hsr, r.hdr, r.vpr, r.vdw, r.vcr,
          a.control, a.x_scroll, a.y_scroll, a.memory, a.hsr, a.hdr, a.vpr, a.vdw, a.vcr})
        w.number(v, 2);
    w.number(s.dot_time, 8);
    w.number(s.physical_line, 8);
    w.number(s.last_line_dot, 8);
    w.number(static_cast<unsigned>(s.h_phase), 1);
    w.number(static_cast<unsigned>(s.v_phase), 1);
    for (auto v :
         {s.h_remaining, s.v_remaining, s.raster, s.y_counter, static_cast<std::uint16_t>(s.row)})
        w.number(v, 2);
    w.number(s.burst, 1);
    w.number(s.y_pending, 1);
    w.number(s.vblank_pending, 1);
    if (w.data.size() != vdc_scan_state_bytes)
        throw std::logic_error("VDC scan wire size mismatch");
    return w.data;
}
bool decode_vdc_scan(const char* data, std::size_t size, rxpce_video::VdcScanState& output,
                     std::string& error) {
    if (!data || size != vdc_scan_state_bytes) {
        error = "Invalid VDC scan length";
        return false;
    }
    try {
        Reader r(data, size);
        rxpce_video::VdcScanState s;
        auto& q = s.requested;
        auto& a = s.active;
        for (auto p : {&q.control, &q.raster, &q.x_scroll, &q.y_scroll, &q.memory, &q.hsr, &q.hdr,
                       &q.vpr, &q.vdw, &q.vcr, &a.control, &a.x_scroll, &a.y_scroll, &a.memory,
                       &a.hsr, &a.hdr, &a.vpr, &a.vdw, &a.vcr})
            *p = static_cast<std::uint16_t>(r.number(2));
        s.dot_time = r.number(8);
        s.physical_line = r.number(8);
        s.last_line_dot = r.number(8);
        s.h_phase = static_cast<rxpce_video::HPeriod>(r.number(1));
        s.v_phase = static_cast<rxpce_video::VPeriod>(r.number(1));
        for (auto p : {&s.h_remaining, &s.v_remaining, &s.raster, &s.y_counter})
            *p = static_cast<std::uint16_t>(r.number(2));
        s.row = static_cast<std::int16_t>(r.signed_number(2));
        s.burst = r.boolean();
        s.y_pending = r.boolean();
        s.vblank_pending = r.boolean();
        rxpce_video::VdcScan checked;
        checked.restore(s);
        output = s;
        error.clear();
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}
} // namespace rxpce_state
