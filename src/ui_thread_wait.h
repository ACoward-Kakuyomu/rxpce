#pragma once
#include <windows.h>

namespace rxpce_ui {
// DXGI may synchronously call the window procedure from the rendering thread.
// Service those calls while joining, leaving queued commands for the main loop.
inline DWORD wait_for_thread(HANDLE thread) {
    for (;;) {
        const DWORD result =
            MsgWaitForMultipleObjectsEx(1, &thread, INFINITE, QS_SENDMESSAGE, MWMO_INPUTAVAILABLE);
        if (result != WAIT_OBJECT_0 + 1)
            return result;
        MSG message;
        PeekMessage(&message, nullptr, 0, 0, PM_NOREMOVE | PM_QS_SENDMESSAGE);
    }
}
} // namespace rxpce_ui
