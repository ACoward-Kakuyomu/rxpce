#pragma once
#include "machine_io.h"
#include <cstddef>
#include <string>
#include <vector>

namespace rxpce_state {
namespace io_wire {
constexpr std::size_t vdc = 0, vce = 64, palette_address = 1088, vdc_increment = 1090,
                      vdc_index = 1092;
constexpr std::size_t dimensions = 1095, controllers = 1119, psg = 1143, wave = 1191,
                      wave_offsets = 1383;
constexpr std::size_t psg_channel = 1389, timer = 1393, irq = 1396, backup = 1398, size = 1400;
} // namespace io_wire
bool valid_io(const rxpce_machine::IoState&, std::string& error);
std::vector<char> encode_io(const rxpce_machine::IoState&);
bool decode_io(const char* data, std::size_t size, rxpce_machine::IoState& output,
               std::string& error);
} // namespace rxpce_state
