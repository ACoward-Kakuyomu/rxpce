#pragma once
#include <windows.h>
#include <atomic>
#include "cd_session.h"
struct IProgressDialog;
void StopCdProgressDialog(HWND owner, IProgressDialog* dialog, bool owner_was_enabled = true);
bool PrepareCdWithProgress(HWND owner, const std::wstring& image, const std::wstring& bios,
                           std::atomic_bool& cancel, std::unique_ptr<rxpce_cd::Session>& result,
                           std::string& error);
