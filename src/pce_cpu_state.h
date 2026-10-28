#pragma once
#include "pce_cpu.h"
#include <vector>
namespace rxpce_state {
constexpr std::size_t cpu_state_size = 47;
std::vector<char> encode_cpu(const PceCpu&);
bool decode_cpu(const char* data, std::size_t size, int line_period, int timer_period,
                PceCpu& result);
} // namespace rxpce_state
