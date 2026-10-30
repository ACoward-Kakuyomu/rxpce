#pragma once
#include "vdc_timing.h"
#include "vdc_dma.h"
#include <cstddef>
#include <string>
#include <vector>
namespace rxpce_state {
struct VdcSnapshot {
    rxpce_video::VdcTimingConfig config;
    rxpce_video::VdcTimingState timing;
    rxpce_video::VdcDmaState dma;
    rxpce_video::CpuVramAccess cpu;
};
constexpr std::size_t vdc_state_bytes = 99;
bool valid_vdc(const VdcSnapshot&, std::string& error);
std::vector<char> encode_vdc(const VdcSnapshot&);
bool decode_vdc(const char*, std::size_t, VdcSnapshot&, std::string& error);
} // namespace rxpce_state
