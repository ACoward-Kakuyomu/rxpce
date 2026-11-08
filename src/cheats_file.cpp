#include "cheats.h"
#include "unique_handle.h"
#include <atomic>
#include <fstream>

namespace rxpce_cheats {
std::filesystem::path sidecar_path(const std::filesystem::path& image) {
    auto path = image;
    path += L".cheats.json";
    return path;
}
bool load(const std::filesystem::path& path, const rxpce_replay::MediaIdentity& expected,
          const rxpce_search::Snapshot& ram, Document& output, std::string& error) {
    const auto attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES &&
        (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND)) {
        output = Document{expected, {}};
        error.clear();
        return true;
    }
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    const auto size = input.tellg();
    if (!input || size < 0 || size > static_cast<std::streamoff>(max_file_bytes)) {
        error = "Cannot read cheat file (limit 1 MiB)";
        return false;
    }
    std::string text(static_cast<std::size_t>(size), '\0');
    input.seekg(0);
    if (!input.read(text.data(), static_cast<std::streamsize>(text.size())) || input.peek() != std::char_traits<char>::eof()) {
        error = "Cheat file changed while reading";
        return false;
    }
    return decode(text, expected, ram, output, error);
}
bool save(const std::filesystem::path& path, const Document& document,
          const rxpce_search::Snapshot& ram, std::string& error) {
    if (!validate(document, ram, error)) return false;
    std::filesystem::path temporary;
    bool created = false;
    try {
        const auto text = encode(document);
        if (text.size() > max_file_bytes) throw std::runtime_error("Cheat JSON exceeds 1 MiB");
        static std::atomic_uint sequence{0};
        temporary = path;
        temporary += L"." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(sequence++) + L".tmp";
        rxpce_host::UniqueHandle file(CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                                                 CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!file) throw std::runtime_error("Cannot create cheat temporary file");
        created = true;
        DWORD written = 0;
        const auto size = static_cast<DWORD>(text.size());
        const bool ready = WriteFile(file.get(), text.data(), size, &written, nullptr) &&
                           written == size && FlushFileBuffers(file.get());
        file.reset();
        if (!ready || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot publish cheat file");
        error.clear();
        return true;
    } catch (const std::exception& failure) {
        if (created) DeleteFileW(temporary.c_str());
        error = failure.what();
        return false;
    }
}
} // namespace rxpce_cheats
