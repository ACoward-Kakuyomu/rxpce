#define NOMINMAX
#include "nvram.h"
#include <windows.h>
#include "unique_handle.h"
#include <vector>
#include <algorithm>
#include <limits>
#include <array>
#include <bcrypt.h>
#ifdef RXPCE_REVIEW_V160_TEST
#include "review_v160_hooks.h"
#endif

namespace {
bool native_path(const std::wstring& path, std::wstring& output) {
    if (path.empty() || path.find(L'\0') != std::wstring::npos)
        return false;
    auto preferred = path;
    std::replace(preferred.begin(), preferred.end(), L'/', L'\\');
    // Resolve once so the temporary and destination cannot use different CWDs.
    std::vector<wchar_t> buffer(32768);
    const auto length = GetFullPathNameW(preferred.c_str(), static_cast<DWORD>(buffer.size()),
                                         buffer.data(), nullptr);
    if (!length || length >= buffer.size() || buffer[length - 1] == L'\\')
        return false;
    output.assign(buffer.data(), length);
    if (output.compare(0, 4, L"\\\\?\\") != 0 && output.compare(0, 4, L"\\\\.\\") != 0) {
        output = output.compare(0, 2, L"\\\\") == 0
                     ? L"\\\\?\\UNC\\" + output.substr(2)
                     : L"\\\\?\\" + output;
    }
    return output.size() < 32767;
}
struct TemporaryFile {
    std::wstring path;
    rxpce_host::UniqueHandle file;
    bool owned = false;
    ~TemporaryFile() {
        file.reset();
        if (owned) DeleteFileW(path.c_str());
    }
};
}
namespace rxpce_storage {
bool load_nvram(const std::wstring& path, unsigned char* ram, std::size_t size,
                std::string& error) {
    if (!ram || !size || size > std::numeric_limits<DWORD>::max()) {
        error = "Invalid RAM buffer size";
        return false;
    }
    std::wstring destination;
    if (!native_path(path, destination)) {
        error = "Cannot resolve RAM file path";
        return false;
    }
    rxpce_host::UniqueHandle file(CreateFileW(destination.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
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
    std::wstring destination;
    if (!native_path(path, destination)) {
        error = "Cannot resolve RAM file path";
        return false;
    }
    const auto directory = destination.substr(0, destination.find_last_of(L'\\') + 1);
    TemporaryFile temporary;
    for (unsigned attempt = 0; attempt < 128; ++attempt) {
        std::array<unsigned char, 16> random{};
        if (BCryptGenRandom(nullptr, random.data(), static_cast<ULONG>(random.size()),
                            BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) {
            error = "Cannot generate RAM temporary filename";
            return false;
        }
        std::wstring name = L".rxpce-ram-";
        constexpr wchar_t hex[] = L"0123456789abcdef";
        for (const auto byte : random) {
            name += hex[byte >> 4]; name += hex[byte & 15];
        }
        temporary.path = directory + name + L".tmp";
#ifdef RXPCE_REVIEW_V160_TEST
        if (rxpce_review_v160::before_nvram_create)
            rxpce_review_v160::before_nvram_create(temporary.path);
#endif
        temporary.file.reset(CreateFileW(temporary.path.c_str(), GENERIC_WRITE, 0, nullptr,
                                          CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (temporary.file) { temporary.owned = true; break; }
        if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS)
            break;
    }
    if (!temporary.file) {
        error = "Cannot create RAM temporary file";
        return false;
    }
    DWORD written = 0;
    bool ok = true;
#ifdef RXPCE_REVIEW_V160_TEST
    if (rxpce_review_v160::allow_nvram)
        ok = rxpce_review_v160::allow_nvram(rxpce_review_v160::NvramPoint::write);
#endif
    ok = ok && WriteFile(temporary.file.get(), ram, static_cast<DWORD>(size), &written, nullptr) && written == size;
#ifdef RXPCE_REVIEW_V160_TEST
    if (ok && rxpce_review_v160::allow_nvram)
        ok = rxpce_review_v160::allow_nvram(rxpce_review_v160::NvramPoint::flush);
#endif
    ok = ok && FlushFileBuffers(temporary.file.get());
    temporary.file.reset();
#ifdef RXPCE_REVIEW_V160_TEST
    if (ok && rxpce_review_v160::before_replace)
        rxpce_review_v160::before_replace(temporary.path, destination);
    if (ok && rxpce_review_v160::allow_nvram)
        ok = rxpce_review_v160::allow_nvram(rxpce_review_v160::NvramPoint::replace);
#endif
    if (ok)
        ok = MoveFileExW(temporary.path.c_str(), destination.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    if (!ok) {
        error = "Cannot replace RAM file";
        return false;
    }
    temporary.owned = false; // The file is now the destination, not our temporary.
    error.clear();
    return true;
}
} // namespace rxpce_storage
