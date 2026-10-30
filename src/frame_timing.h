#pragma once
void InitializeFrameTiming(unsigned frame_cycles = 1, unsigned clock_hz = 60);
void ShutdownFrameTiming();
void ResetFrameTiming();
bool BeginVideoFrame(int minimum_skip, bool automatic);
