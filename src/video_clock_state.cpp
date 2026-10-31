#include "video_clock_state.h"
#include "state_codec.h"
namespace rxpce_state {
namespace {
void validate(const VideoClockSnapshot& s) {
    rxpce_video::CpuVideoClock cpu(s.cpu_hz);
    cpu.restore(s.cpu);
    rxpce_video::VceClock vce;
    vce.restore(s.vce);
    if (s.cpu.master_time != s.vce.master_time)
        throw std::invalid_argument("CPU/VCE master clock mismatch");
}
} // namespace
std::vector<char> encode_video_clock(const VideoClockSnapshot& s) {
    validate(s);
    Writer w;
    w.data.reserve(video_clock_state_bytes);
    w.number(s.cpu_hz, 4);
    w.number(s.cpu.cpu_time, 8);
    w.number(s.cpu.master_time, 8);
    w.number(s.cpu.remainder, 4);
    w.number(s.vce.master_time, 8);
    w.number(s.vce.frame_number, 8);
    w.number(s.vce.dots, 8);
    w.number(s.vce.line, 2);
    w.number(s.vce.line_master, 2);
    w.number(s.vce.frame_lines, 2);
    w.number(s.vce.control, 1);
    w.number(s.vce.dot_remainder, 1);
    if (w.data.size() != video_clock_state_bytes)
        throw std::logic_error("Video clock wire size mismatch");
    return w.data;
}
bool decode_video_clock(const char* data, std::size_t size, std::uint32_t expected_cpu_hz,
                        VideoClockSnapshot& output, std::string& error) {
    if (!data || size != video_clock_state_bytes) {
        error = "Invalid video clock state length";
        return false;
    }
    try {
        Reader r(data, size);
        VideoClockSnapshot s;
        s.cpu_hz = static_cast<std::uint32_t>(r.number(4));
        s.cpu.cpu_time = r.number(8);
        s.cpu.master_time = r.number(8);
        s.cpu.remainder = static_cast<std::uint32_t>(r.number(4));
        s.vce.master_time = r.number(8);
        s.vce.frame_number = r.number(8);
        s.vce.dots = r.number(8);
        s.vce.line = static_cast<std::uint16_t>(r.number(2));
        s.vce.line_master = static_cast<std::uint16_t>(r.number(2));
        s.vce.frame_lines = static_cast<std::uint16_t>(r.number(2));
        s.vce.control = static_cast<std::uint8_t>(r.number(1));
        s.vce.dot_remainder = static_cast<std::uint8_t>(r.number(1));
        if (s.cpu_hz != expected_cpu_hz)
            throw std::invalid_argument("Video clock CPU frequency mismatch");
        validate(s);
        output = s;
        error.clear();
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}
} // namespace rxpce_state
