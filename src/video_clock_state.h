#pragma once
#include "vce_clock.h"
#include <cstddef>
#include <string>
#include <vector>
namespace rxpce_state {
inline constexpr std::size_t video_clock_state_bytes = 56;
struct VideoClockSnapshot {
    std::uint32_t cpu_hz = 7170000;
    rxpce_video::CpuVideoState cpu;
    rxpce_video::VceClockState vce;
};
std::vector<char> encode_video_clock(const VideoClockSnapshot&);
bool decode_video_clock(const char*, std::size_t, std::uint32_t expected_cpu_hz,
                        VideoClockSnapshot&, std::string& error);
} // namespace rxpce_state
