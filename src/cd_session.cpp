#define NOMINMAX
#include "cd_session.h"
#include <windows.h>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace rxpce_cd {
namespace {
std::mutex pending_mutex;
std::unique_ptr<Session> pending;
} // namespace
std::wstring bios_path(const std::wstring& path) {
    if (path.empty())
        throw std::runtime_error("Select a System Card BIOS in Settings / Folders");
    if (path.find(L'\0') != std::wstring::npos)
        throw std::runtime_error("Invalid BIOS path");
    const bool absolute =
        (path.size() > 2 && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/')) ||
        (path.size() > 1 && path[0] == L'\\' && path[1] == L'\\');
    if (absolute)
        return path;
    if (path[0] == L'/' || path[0] == L'\\' || (path.size() > 1 && path[1] == L':'))
        throw std::runtime_error("Use an absolute or executable-relative BIOS path");
    std::vector<wchar_t> executable(32768);
    const auto length =
        GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (!length || length >= executable.size())
        throw std::runtime_error("Cannot resolve executable folder");
    const std::wstring full(executable.data(), length);
    return full.substr(0, full.find_last_of(L"/\\") + 1) + path;
}
bool prepare(const std::wstring& image, const std::wstring& bios, std::unique_ptr<Session>& output,
             std::string& error, const ImageProgress& progress) {
    try {
        auto next = std::make_unique<Session>();
        if (image.empty() || image.find(L'\0') != std::wstring::npos)
            throw std::runtime_error("Invalid CD image path");
        const auto needed = GetFullPathNameW(image.c_str(), 0, nullptr, nullptr);
        if (!needed)
            throw std::runtime_error("Cannot resolve CD image path");
        std::vector<wchar_t> full(needed);
        const auto length = GetFullPathNameW(image.c_str(), needed, full.data(), nullptr);
        if (!length || length >= needed)
            throw std::runtime_error("Cannot resolve CD image path");
        next->source.assign(full.data(), length);
        const auto dot = next->source.find_last_of(L'.');
        auto extension = dot == std::wstring::npos ? std::wstring{} : next->source.substr(dot);
        for (auto& ch : extension)
            if (ch >= L'A' && ch <= L'Z')
                ch += L'a' - L'A';
        if (extension != L".cue" && extension != L".chd")
            throw std::runtime_error("Expected CUE or CHD image");
        if (!load_system_card(bios_path(bios), next->bios, error))
            return false;
        if (extension == L".chd" ? !next->disc.open_chd(next->source, error, progress)
                                 : !next->disc.open_cue(next->source, error, progress))
            return false;
        output = std::move(next);
        error.clear();
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
}
void queue(std::unique_ptr<Session> session) {
    std::lock_guard<std::mutex> lock(pending_mutex);
    pending = std::move(session);
}
std::unique_ptr<Session> take() {
    std::lock_guard<std::mutex> lock(pending_mutex);
    return std::move(pending);
}
} // namespace rxpce_cd
