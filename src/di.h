#pragma once
#include <windows.h>
#include "input_state.h"

BOOL InitDInput();
BOOL ReadJoystick(unsigned player, rxpce_input::PadSnapshot& state);
BOOL ReleaseDInput();
BOOL ReleaseDevices();
