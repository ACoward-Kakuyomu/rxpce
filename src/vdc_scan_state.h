#pragma once
#include "vdc_scan.h"
#include <cstddef>
#include <string>
#include <vector>
namespace rxpce_state {
inline constexpr std::size_t vdc_scan_state_bytes = 77;
std::vector<char> encode_vdc_scan(const rxpce_video::VdcScanState&);
bool decode_vdc_scan(const char*, std::size_t, rxpce_video::VdcScanState&, std::string& error);
} // namespace rxpce_state
