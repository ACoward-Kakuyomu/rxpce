#pragma once
#include <windows.h>
#include "audio_output.h"
BOOL InitDSound(HWND window, const rxpce_audio::OutputSettings& settings);
BOOL ReleaseDSound();
