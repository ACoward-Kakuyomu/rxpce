#pragma once
#include <windows.h>
#include <shellapi.h>
#include <string>

namespace rxpce_ui {
struct DropState {
    bool initialized=false, closing=false, opening=false, preparing=false, stopping=false,
         dialog=false, searching=false, replay=false, pending_recording=false, worker_starting=false;
};
inline bool can_drop_game(const DropState& state) noexcept {
    return state.initialized && !state.closing && !state.opening && !state.preparing &&
           !state.stopping && !state.dialog && !state.searching && !state.replay &&
           !state.pending_recording && !state.worker_starting;
}
enum class DropError { none, busy, count, path, unsupported };
struct DropSelection {std::wstring path;DropError error=DropError::none;};
// Takes ownership, including rejected/busy inputs; finishes before returning or throwing.
DropSelection read_game_drop(HDROP drop,bool available=true);
}
