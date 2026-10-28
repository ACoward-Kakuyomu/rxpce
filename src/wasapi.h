#pragma once
#include <windows.h>
BOOL InitWasapi(unsigned rate, unsigned channels, unsigned latency_ms);
void ReleaseWasapi();
