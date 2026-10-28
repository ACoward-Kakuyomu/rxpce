#pragma once
void InitializeFrameTiming();
void ShutdownFrameTiming();
void ResetFrameTiming();
bool BeginVideoFrame(int minimum_skip, bool automatic);
