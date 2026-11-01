#pragma once
#include "media_access.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <functional>
#include <string>
#include <vector>
namespace rxpce_cd::detail {
struct LocalMediaPath {
    std::wstring volume, dos_root;
    std::vector<std::wstring> parts;
};
using DosDeviceQuery = std::function<std::wstring(const std::wstring&)>;
// Pure classification: never opens the path or follows filesystem reparse points.
LocalMediaPath local_media_path(const std::wstring& absolute, const DosDeviceQuery& query);
std::wstring media_link_target(const void* bytes, std::size_t size, const std::wstring& parent);
// Caller owns the returned handle. The checked file is not reopened by pathname.
HANDLE open_media_file(const std::wstring& path, MediaAccess access);
} // namespace rxpce_cd::detail
