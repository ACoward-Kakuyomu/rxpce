#pragma once
#include "vdc_scan_state.h"
#include "vdc_dma.h"
#include <cstddef>
#include <string>
#include <vector>
namespace rxpce_state {
struct VdcSnapshot {
    rxpce_video::VdcScanState scan;
    rxpce_video::VdcDmaState dma;
    rxpce_video::CpuVramAccess cpu;
    std::uint8_t sprite_fetch_cells = 0;
    std::uint16_t sprite_fetch_remaining = 0;
    std::uint8_t fast_satb_lines = 0;
};
constexpr std::size_t vdc_state_bytes = 121;
bool valid_vdc(const VdcSnapshot&, std::string& error);
std::vector<char> encode_vdc(const VdcSnapshot&);
bool decode_vdc(const char*, std::size_t, VdcSnapshot&, std::string& error);
} // namespace rxpce_state
