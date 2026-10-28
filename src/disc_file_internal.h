#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>

namespace rxpce_cd::detail {
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    Handle() = default;
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    ~Handle() {
        if (value != INVALID_HANDLE_VALUE)
            CloseHandle(value);
    }
};
struct Algorithm {
    BCRYPT_ALG_HANDLE value = nullptr;
    Algorithm() = default;
    Algorithm(const Algorithm&) = delete;
    Algorithm& operator=(const Algorithm&) = delete;
    ~Algorithm() {
        if (value)
            BCryptCloseAlgorithmProvider(value, 0);
    }
};
struct HashHandle {
    BCRYPT_HASH_HANDLE value = nullptr;
    HashHandle() = default;
    HashHandle(const HashHandle&) = delete;
    HashHandle& operator=(const HashHandle&) = delete;
    ~HashHandle() {
        if (value)
            BCryptDestroyHash(value);
    }
};
class Sha256 {
    Algorithm algorithm_;
    HashHandle hash_;

  public:
    Sha256() {
        if (BCryptOpenAlgorithmProvider(&algorithm_.value, BCRYPT_SHA256_ALGORITHM, nullptr, 0) <
                0 ||
            BCryptCreateHash(algorithm_.value, &hash_.value, nullptr, 0, nullptr, 0, 0) < 0)
            throw std::runtime_error("Cannot initialize disc SHA-256");
    }
    void add(const void* bytes, std::size_t count) {
        if (count > std::numeric_limits<ULONG>::max() ||
            BCryptHashData(hash_.value,
                           const_cast<PUCHAR>(static_cast<const unsigned char*>(bytes)),
                           static_cast<ULONG>(count), 0) < 0)
            throw std::runtime_error("Cannot hash disc image");
    }
    void integer(std::uint64_t value) {
        unsigned char bytes[8];
        for (unsigned n = 0; n < 8; ++n)
            bytes[n] = static_cast<unsigned char>(value >> (8 * n));
        add(bytes, sizeof(bytes));
    }
    std::string finish() {
        unsigned char bytes[32];
        if (BCryptFinishHash(hash_.value, bytes, sizeof(bytes), 0) < 0)
            throw std::runtime_error("Cannot finish disc SHA-256");
        const char hex[] = "0123456789abcdef";
        std::string result;
        for (auto value : bytes) {
            result += hex[value >> 4];
            result += hex[value & 15];
        }
        return result;
    }
};
struct File {
    Handle handle;
    std::uint64_t bytes = 0;
    BY_HANDLE_FILE_INFORMATION info{};
    explicit File(const std::wstring& path) {
        handle.value = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle.value == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Cannot open referenced file (Windows error " +
                                     std::to_string(GetLastError()) + ")");
        LARGE_INTEGER size{};
        if (GetFileType(handle.value) != FILE_TYPE_DISK || !GetFileSizeEx(handle.value, &size) ||
            size.QuadPart < 0 || !GetFileInformationByHandle(handle.value, &info))
            throw std::runtime_error("Cannot inspect referenced file");
        bytes = static_cast<std::uint64_t>(size.QuadPart);
    }
    bool same_file(const File& other) const {
        return (info.nFileIndexHigh || info.nFileIndexLow) &&
               info.dwVolumeSerialNumber == other.info.dwVolumeSerialNumber &&
               info.nFileIndexHigh == other.info.nFileIndexHigh &&
               info.nFileIndexLow == other.info.nFileIndexLow;
    }
    void read(std::uint64_t offset, unsigned char* output, unsigned count) {
        if (offset > bytes || count > bytes - offset)
            throw std::runtime_error("Read outside referenced file");
        if (!count)
            return;
        LARGE_INTEGER position{};
        position.QuadPart = static_cast<LONGLONG>(offset);
        DWORD received = 0;
        if (!SetFilePointerEx(handle.value, position, nullptr, FILE_BEGIN) ||
            !ReadFile(handle.value, output, count, &received, nullptr) || received != count)
            throw std::runtime_error("Cannot read referenced file (Windows error " +
                                     std::to_string(GetLastError()) + ")");
    }
};
inline std::wstring full_path(const std::wstring& path) {
    if (path.empty() || path.find(L'\0') != std::wstring::npos)
        throw std::runtime_error("Invalid image path");
    const auto size = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (!size)
        throw std::runtime_error("Cannot resolve image path");
    std::vector<wchar_t> buffer(size);
    const auto length = GetFullPathNameW(path.c_str(), size, buffer.data(), nullptr);
    if (!length || length >= size)
        throw std::runtime_error("Cannot resolve image path");
    return {buffer.data(), length};
}
} // namespace rxpce_cd::detail
