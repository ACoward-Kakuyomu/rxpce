#include "media_file.h"
#include <winternl.h>
#include <winioctl.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace rxpce_cd::detail {
namespace {
bool starts(const std::wstring& s, const std::wstring& prefix) {
    if (s.size() < prefix.size())
        return false;
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        auto upper = [](wchar_t c) { return c >= L'a' && c <= L'z' ? c - L'a' + L'A' : c; };
        if (upper(s[i]) != upper(prefix[i]))
            return false;
    }
    return true;
}
[[noreturn]] void blocked() {
    throw std::runtime_error(
        "Network or unclassified CD media is blocked by the local-only policy");
}
bool local_device(const std::wstring& s) {
    for (const auto* prefix : {L"\\Device\\HarddiskVolume", L"\\Device\\CdRom"}) {
        const std::wstring p(prefix);
        if (starts(s, p) && s.size() > p.size() &&
            std::all_of(s.begin() + p.size(), s.end(),
                        [](wchar_t c) { return c >= L'0' && c <= L'9'; }))
            return true;
    }
    return false;
}
std::wstring dos_name(std::wstring s) {
    if (starts(s, L"\\??\\Volume{"))
        return L"\\\\?\\" + s.substr(4);
    if (starts(s, L"\\??\\UNC\\"))
        return L"\\\\" + s.substr(8);
    if (starts(s, L"\\??\\"))
        return s.substr(4);
    return s;
}
std::wstring full_path(const std::wstring& path) {
    if (path.empty() || path.find(L'\0') != std::wstring::npos)
        throw std::runtime_error("Invalid CD media path");
    const auto size = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (!size || size > 32768)
        throw std::runtime_error("Cannot resolve CD media path");
    std::vector<wchar_t> result(size);
    const auto n = GetFullPathNameW(path.c_str(), size, result.data(), nullptr);
    if (!n || n >= size)
        throw std::runtime_error("Cannot resolve CD media path");
    return {result.data(), n};
}
struct Owned {
    HANDLE value = INVALID_HANDLE_VALUE;
    explicit Owned(HANDLE h) : value(h) {}
    ~Owned() {
        if (value != INVALID_HANDLE_VALUE)
            CloseHandle(value);
    }
    Owned(const Owned&) = delete;
    Owned& operator=(const Owned&) = delete;
    void reset(HANDLE h) {
        if (value != INVALID_HANDLE_VALUE)
            CloseHandle(value);
        value = h;
    }
    HANDLE release() {
        return std::exchange(value, INVALID_HANDLE_VALUE);
    }
};
HANDLE native_open(HANDLE parent, const std::wstring& name, bool directory) {
    if (name.size() > 32766)
        throw std::runtime_error("CD media component is too long");
    const auto module = GetModuleHandleW(L"ntdll.dll");
    const auto address = module ? GetProcAddress(module, "NtCreateFile") : nullptr;
    decltype(&NtCreateFile) create = nullptr;
    static_assert(sizeof(create) == sizeof(address));
    std::memcpy(&create, &address, sizeof(create));
    if (!create)
        throw std::runtime_error("Local CD media access is unavailable");
    UNICODE_STRING text{};
    text.Buffer = const_cast<wchar_t*>(name.data());
    text.Length = static_cast<USHORT>(name.size() * sizeof(wchar_t));
    text.MaximumLength = text.Length;
    OBJECT_ATTRIBUTES attrs{};
    attrs.Length = sizeof(attrs);
    attrs.RootDirectory = parent;
    attrs.ObjectName = &text;
    attrs.Attributes = OBJ_CASE_INSENSITIVE;
    IO_STATUS_BLOCK status{};
    HANDLE result = INVALID_HANDLE_VALUE;
    // One component at a time. FILE_OPEN_REPARSE_POINT applies to that component,
    // rather than permitting Windows to follow an unchecked ancestor.
    constexpr ULONG open_reparse = 0x00200000, synchronous = 0x20, directory_file = 1;
    const auto wanted = directory ? FILE_READ_ATTRIBUTES | SYNCHRONIZE : FILE_GENERIC_READ;
    const auto code =
        create(&result, wanted, &attrs, &status, nullptr, 0,
               directory ? FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE : FILE_SHARE_READ,
               1, open_reparse | synchronous | (directory ? directory_file : 0), nullptr, 0);
    if (code < 0)
        throw std::runtime_error("Cannot open local CD media (status " +
                                 std::to_string(static_cast<unsigned long>(code)) + ")");
    return result;
}
std::wstring query_device(const std::wstring& name) {
    std::vector<wchar_t> target(32768);
    if (!QueryDosDeviceW(name.c_str(), target.data(), static_cast<DWORD>(target.size())))
        blocked();
    return target.data();
}
std::uint16_t u16(const unsigned char* p) {
    return std::uint16_t(p[0] | unsigned(p[1]) << 8);
}
std::uint32_t u32(const unsigned char* p) {
    return std::uint32_t(u16(p)) | std::uint32_t(u16(p + 2)) << 16;
}
} // namespace
LocalMediaPath local_media_path(const std::wstring& absolute, const DosDeviceQuery& query) {
    auto path = absolute;
    std::replace(path.begin(), path.end(), L'/', L'\\');
    for (unsigned aliases = 0; aliases < 64; ++aliases) {
        if (path.empty() || path.find(L'\0') != std::wstring::npos)
            blocked();
        if (starts(path, L"\\\\?\\"))
            path.erase(0, 4);
        std::wstring drive, root;
        std::size_t offset = 0;
        if (path.size() >= 3 &&
            ((path[0] >= L'A' && path[0] <= L'Z') || (path[0] >= L'a' && path[0] <= L'z')) &&
            path[1] == L':' && path[2] == L'\\') {
            drive = path.substr(0, 2);
            if (drive[0] >= L'a' && drive[0] <= L'z')
                drive[0] -= L'a' - L'A';
            root = drive + L"\\";
            offset = 3;
        } else if (starts(path, L"Volume{")) {
            const auto end = path.find(L"}\\");
            if (end == std::wstring::npos)
                blocked();
            drive = path.substr(0, end + 1);
            root = L"\\\\?\\" + drive + L"\\";
            offset = end + 2;
        } else
            blocked();
        LocalMediaPath p;
        p.dos_root = root;
        while (offset <= path.size()) {
            const auto end = path.find(L'\\', offset);
            const auto part = path.substr(offset, end == std::wstring::npos ? end : end - offset);
            if (part == L"..") {
                if (!p.parts.empty())
                    p.parts.pop_back();
            } else if (!part.empty() && part != L".")
                p.parts.push_back(part);
            if (end == std::wstring::npos)
                break;
            offset = end + 1;
        }
        auto target = query(drive);
        if (local_device(target)) {
            p.volume = std::move(target);
            return p;
        }
        if (!starts(target, L"\\??\\"))
            blocked();
        path = dos_name(target);
        for (const auto& part : p.parts) {
            if (path.back() != L'\\')
                path += L'\\';
            path += part;
        }
    }
    throw std::runtime_error("Too many CD media drive aliases");
}
std::wstring media_link_target(const void* bytes, std::size_t size, const std::wstring& parent) {
    const auto* p = static_cast<const unsigned char*>(bytes);
    if (!p || size < 16)
        throw std::runtime_error("Invalid CD media link");
    const auto tag = u32(p);
    const auto total = std::size_t(u16(p + 4)) + 8;
    const bool symbolic = tag == IO_REPARSE_TAG_SYMLINK;
    if (!symbolic && tag != IO_REPARSE_TAG_MOUNT_POINT)
        blocked();
    const auto base = symbolic ? 20u : 16u;
    if (total > size || total < base)
        throw std::runtime_error("Invalid CD media link size");
    const auto offset = u16(p + 8), length = u16(p + 10);
    if ((offset | length) & 1 || !length || offset > total - base || length > total - base - offset)
        throw std::runtime_error("Invalid CD media link name");
    std::wstring target(length / 2, L'\0');
    std::memcpy(target.data(), p + base + offset, length);
    if (target.find(L'\0') != std::wstring::npos)
        throw std::runtime_error("Invalid CD media link name");
    const auto flags = symbolic ? u32(p + 16) : 0;
    if (flags & ~1u)
        blocked();
    if (flags & 1) {
        if (target[0] == L'\\' || target[0] == L'/' || (target.size() > 1 && target[1] == L':'))
            blocked();
        return parent + target;
    }
    if (!starts(target, L"\\??\\"))
        blocked();
    const auto result = dos_name(target);
    const bool drive = result.size() >= 3 && result[1] == L':' && result[2] == L'\\';
    if (!drive && !starts(result, L"\\\\"))
        blocked();
    return result;
}
HANDLE open_media_file(const std::wstring& source, MediaAccess access) {
    auto path = full_path(source);
    if (access == MediaAccess::allow_network) {
        const auto file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Cannot open CD media (Windows error " +
                                     std::to_string(GetLastError()) + ")");
        return file;
    }
    if (access != MediaAccess::local_only)
        blocked();
    for (unsigned links = 0; links < 64; ++links) {
        const auto p = local_media_path(path, query_device);
        if (p.parts.empty())
            throw std::runtime_error("CD media path names a directory");
        Owned current(native_open(nullptr, p.volume + L"\\", true));
        auto parent = p.dos_root;
        bool restart = false;
        for (std::size_t n = 0; n < p.parts.size(); ++n) {
            const bool last = n + 1 == p.parts.size();
            Owned next(native_open(current.value, p.parts[n], !last));
            FILE_ATTRIBUTE_TAG_INFO info{};
            if (!GetFileInformationByHandleEx(next.value, FileAttributeTagInfo, &info,
                                              sizeof(info)))
                throw std::runtime_error("Cannot classify CD media component");
            if (info.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
                std::vector<unsigned char> data(MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
                DWORD received = 0;
                if (!DeviceIoControl(next.value, FSCTL_GET_REPARSE_POINT, nullptr, 0, data.data(),
                                     static_cast<DWORD>(data.size()), &received, nullptr))
                    throw std::runtime_error("Cannot inspect CD media link");
                path = media_link_target(data.data(), received, parent);
                for (std::size_t k = n + 1; k < p.parts.size(); ++k) {
                    if (path.back() != L'\\')
                        path += L'\\';
                    path += p.parts[k];
                }
                path = full_path(path);
                restart = true;
                break;
            }
            if (last) {
                if (info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    throw std::runtime_error("CD media path names a directory");
                return next.release();
            }
            current.reset(next.release());
            parent += p.parts[n] + L"\\";
        }
        if (!restart)
            throw std::runtime_error("Cannot resolve CD media file");
    }
    throw std::runtime_error("Too many CD media links");
}
} // namespace rxpce_cd::detail
