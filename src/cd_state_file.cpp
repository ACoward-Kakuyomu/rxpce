#define NOMINMAX
#include "cd_state_file.h"
#include "nvram.h"
#include <windows.h>
#include "unique_handle.h"
namespace rxpce_cd {
void write_binding(rxpce_state::Writer& w, const StateBinding& b) {
    if (b.bios.size() != 64 || b.disc.size() != 64)
        throw std::runtime_error("Invalid CD identity");
    w.number(b.kind, 4);
    w.number(b.clock, 4);
    w.block(b.bios.data(), 64);
    w.block(b.disc.data(), 64);
}
bool read_binding(rxpce_state::Reader& r, const StateBinding& expected, std::string& error) {
    try {
        const auto kind = r.number(4), clock = r.number(4);
        std::string bios(64, 0), disc(64, 0);
        r.block(&bios[0], 64);
        r.block(&disc[0], 64);
        if (kind != expected.kind)
            error = "CD state machine configuration mismatch";
        else if (clock != expected.clock)
            error = "CD state base clock mismatch";
        else if (bios != expected.bios)
            error = "CD state BIOS mismatch";
        else if (disc != expected.disc)
            error = "CD state disc mismatch";
        else {
            error.clear();
            return true;
        }
    } catch (const std::exception& ex) {
        error = ex.what();
    }
    return false;
}
std::wstring state_path(const std::wstring& image, const std::wstring& folder) {
    const auto slash = image.find_last_of(L"/\\");
    const auto base = slash == std::wstring::npos ? 0 : slash + 1;
    auto name = image.substr(base);
    const auto dot = name.find_last_of(L'.');
    auto extension = dot == std::wstring::npos ? std::wstring{} : name.substr(dot);
    for (auto& ch : extension)
        if (ch >= L'A' && ch <= L'Z')
            ch += L'a' - L'A';
    if (dot != std::wstring::npos)
        name.resize(dot);
    if (extension == L".chd")
        name += L".chd";
    const auto directory =
        folder.empty() || folder == L"."
            ? image.substr(0, base)
            : folder + ((folder.back() == L'/' || folder.back() == L'\\') ? L"" : L"\\");
    return directory + name + L".st0";
}
bool read_state_file(const std::wstring& path, std::size_t size, std::vector<char>& output,
                     std::string& error) {
    if (!size || size > 2 * 1024 * 1024) {
        error = "Invalid CD state size";
        return false;
    }
    rxpce_host::UniqueHandle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file) {
        error = "Cannot open CD state file";
        return false;
    }
    std::vector<char> next(size);
    LARGE_INTEGER length{};
    DWORD count = 0;
    const bool ok = GetFileSizeEx(file.get(), &length) &&
                    length.QuadPart == static_cast<LONGLONG>(size) &&
                    ReadFile(file.get(), next.data(), static_cast<DWORD>(size), &count, nullptr) &&
                    count == size;
    file.reset();
    if (!ok) {
        error = "Invalid CD state file length or read failure";
        return false;
    }
    output.swap(next);
    error.clear();
    return true;
}
bool write_state_file(const std::wstring& path, const std::vector<char>& data, std::string& error) {
    const auto slash = path.find_last_of(L"/\\");
    if (slash != std::wstring::npos) {
        const auto folder = path.substr(0, slash);
        const auto attributes = GetFileAttributesW(folder.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES && !CreateDirectoryW(folder.c_str(), nullptr)) {
            error = "Cannot create state folder";
            return false;
        }
    }
    return rxpce_storage::save_nvram(path, reinterpret_cast<const unsigned char*>(data.data()),
                                     data.size(), error);
}
} // namespace rxpce_cd
