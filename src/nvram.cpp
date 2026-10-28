#define NOMINMAX
#include "nvram.h"
#include <windows.h>
#include "unique_handle.h"
#include <vector>
#include <algorithm>
#include <limits>

namespace rxpce_storage {
bool load_nvram(const std::wstring& path, unsigned char* ram, std::size_t size,
                std::string& error) {
    if (!ram || !size || size > std::numeric_limits<DWORD>::max()) {
        error = "Invalid RAM buffer size";
        return false;
    }
    rxpce_host::UniqueHandle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            std::fill_n(ram, size, 0);
            error.clear();
            return true;
        }
        error = "Cannot open RAM file";
        return false;
    }
    std::vector<unsigned char> next(size);
    LARGE_INTEGER length{};
    DWORD read = 0;
    const bool ok =
        GetFileSizeEx(file.get(), &length) && length.QuadPart == static_cast<LONGLONG>(size) &&
        ReadFile(file.get(), next.data(), static_cast<DWORD>(size), &read, nullptr) && read == size;
    file.reset();
    if (!ok) {
        error = "Invalid size or unreadable RAM file";
        return false;
    }
    std::copy(next.begin(), next.end(), ram);
    error.clear();
    return true;
}
bool save_nvram(const std::wstring& path, const unsigned char* ram, std::size_t size,
                std::string& error) {
    if (!ram || !size || size > std::numeric_limits<DWORD>::max()) {
        error = "Invalid RAM buffer size";
        return false;
    }
    const auto temporary = path + L".tmp";
    rxpce_host::UniqueHandle file(CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file) {
        error = "Cannot create RAM temporary file";
        return false;
    }
    DWORD written = 0;
    bool ok = WriteFile(file.get(), ram, static_cast<DWORD>(size), &written, nullptr) &&
              written == size && FlushFileBuffers(file.get());
    file.reset();
    if (ok)
        ok = MoveFileExW(temporary.c_str(), path.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    if (!ok) {
        DeleteFileW(temporary.c_str());
        error = "Cannot replace RAM file";
        return false;
    }
    error.clear();
    return true;
}
} // namespace rxpce_storage
