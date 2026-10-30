#pragma once
#include "machine_io.h"
#include <cstddef>
#include <string>
#include <vector>

namespace rxpce_state {
namespace io_wire {
constexpr std::size_t vdc = 0, vce = 64, palette_address = 1088, vdc_increment = 1090,
                      vdc_index = 1092;
constexpr std::size_t dimensions = 1096, controllers = 1120, psg = 1144, wave = 1192,
                      wave_offsets = 1384;
constexpr std::size_t psg_channel = 1390, timer = 1394, irq = 1397, backup = 1399, size = 1401;
} // namespace io_wire
bool valid_io(const rxpce_machine::IoState&, std::string& error);
std::vector<char> encode_io(const rxpce_machine::IoState&);
bool decode_io(const char* data, std::size_t size, rxpce_machine::IoState& output,
               std::string& error);
} // namespace rxpce_state
