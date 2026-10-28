#pragma once
#include "machine_io.h"
#include <cstddef>
#include <string>
#include <vector>

namespace rxpce_state {
namespace io_wire {
constexpr std::size_t vdc = 0, vce = 64, palette_address = 1088, vdc_increment = 1090,
                      vdc_index = 1092;
constexpr std::size_t dimensions = 1097, controllers = 1121, psg = 1145, wave = 1193,
                      wave_offsets = 1385;
constexpr std::size_t psg_channel = 1391, timer = 1395, irq = 1398, backup = 1400, size = 1402;
} // namespace io_wire
bool valid_io(const rxpce_machine::IoState&, std::string& error);
std::vector<char> encode_io(const rxpce_machine::IoState&);
bool decode_io(const char* data, std::size_t size, rxpce_machine::IoState& output,
               std::string& error);
} // namespace rxpce_state
