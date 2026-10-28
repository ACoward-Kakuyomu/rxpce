#include "file_paths.h"
#include <windows.h>
#include <stdexcept>
#include <vector>

namespace rxpce_paths {
std::filesystem::path executable() {
    std::vector<wchar_t> buffer(1024);
    for (;;) {
        const auto length =
            GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (!length)
            throw std::runtime_error("Cannot locate executable directory");
        if (length < buffer.size())
            return std::filesystem::path(std::wstring(buffer.data(), length));
        if (buffer.size() >= 32768)
            throw std::runtime_error("Executable path exceeds Windows path limit");
        buffer.resize(buffer.size() * 2);
    }
}
} // namespace rxpce_paths
