#include "replay_codec.h"
#include "unique_handle.h"
#include <atomic>
#include <fstream>

namespace rxpce_replay {
bool load_file(const std::filesystem::path& path, Recording& recording, std::string& error) {
    try {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        const auto size = input.tellg();
        if (size <= 0 || size > static_cast<std::streamoff>(max_file_bytes))
            throw std::runtime_error("Unreadable replay or invalid file length");
        std::vector<char> bytes(static_cast<std::size_t>(size));
        input.seekg(0);
        if (!input.read(bytes.data(), static_cast<std::streamsize>(bytes.size())))
            throw std::runtime_error("Cannot read replay");
        char extra;
        if (input.get(extra) || !input.eof())
            throw std::runtime_error("Replay changed while reading");
        return decode(bytes, recording, error);
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}
bool save_file(const std::filesystem::path& path, const Recording& recording, bool replace,
               std::string& error) {
    try {
        const auto bytes = encode(recording);
        static std::atomic_uint sequence{0};
        auto temporary = path;
        temporary += L"." + std::to_wstring(GetCurrentProcessId()) + L"." +
                     std::to_wstring(sequence++) + L".tmp";
        rxpce_host::UniqueHandle file(CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                                                 CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!file)
            throw std::runtime_error("Cannot create replay temporary file");
        DWORD written = 0;
        const auto length = static_cast<DWORD>(bytes.size());
        const bool ready = WriteFile(file.get(), bytes.data(), length, &written, nullptr) &&
                           written == length && FlushFileBuffers(file.get());
        file.reset();
        const DWORD flags = MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0);
        if (!ready || !MoveFileExW(temporary.c_str(), path.c_str(), flags)) {
            DeleteFileW(temporary.c_str());
            throw std::runtime_error("Cannot write or publish replay file");
        }
        error.clear();
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}
} // namespace rxpce_replay
