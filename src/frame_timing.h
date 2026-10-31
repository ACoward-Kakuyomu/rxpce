#pragma once
#include <cstdint>
void InitializeFrameTiming(unsigned frame_cycles = 1, unsigned clock_hz = 60);
void ShutdownFrameTiming();
void ResetFrameTiming();
bool BeginVideoFrame(int minimum_skip, bool automatic, std::uint64_t completed_master_time);
