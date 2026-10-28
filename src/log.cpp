#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "log.h"
#include "file_paths.h"
#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct LogState {
    std::mutex mutex;
    bool detail = true, prepared = false;
    std::filesystem::path path;
};
LogState& state() {
    static LogState value;
    return value;
}
std::string format_message(const char* format, std::va_list args) {
    if (!format)
        return {};
    std::va_list count_args;
    va_copy(count_args, args);
    const int count = std::vsnprintf(nullptr, 0, format, count_args);
    va_end(count_args);
    if (count < 0)
        return {};
    std::string text(static_cast<std::size_t>(count) + 1, '\0');
    std::va_list write_args;
    va_copy(write_args, args);
    const int written = std::vsnprintf(text.data(), text.size(), format, write_args);
    va_end(write_args);
    if (written != count)
        return {};
    text.resize(static_cast<std::size_t>(count));
    return text;
}
void prepare(LogState& log) {
    log.path = rxpce_paths::executable().parent_path() / L"rxpce.log";
    std::error_code error;
    if (std::filesystem::exists(log.path, error)) {
        auto backup = log.path;
        backup += L".bak";
        std::filesystem::remove(backup, error);
        std::filesystem::rename(log.path, backup, error);
    }
    log.prepared = true;
}
#ifdef _DEBUG
void debug_message(const std::string& text) {
    const int length = static_cast<int>(text.size());
    const int count =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), length, nullptr, 0);
    if (count > 0) {
        std::wstring wide(static_cast<std::size_t>(count), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), length, wide.data(), count);
        OutputDebugStringW(wide.c_str());
    } else
        OutputDebugStringA(text.c_str());
}
#endif
void write_message(bool detail, const char* format, std::va_list args) {
    auto& log = state();
    std::lock_guard<std::mutex> lock(log.mutex);
    if (detail && !log.detail)
        return;
    const auto text = format_message(format, args);
    if (!log.prepared)
        prepare(log);
    std::ofstream file(log.path, std::ios::binary | std::ios::app);
    if (file)
        file.write(text.data(), static_cast<std::streamsize>(text.size()));
#ifdef _DEBUG
    debug_message(text);
#endif
}
} // namespace
void SetLogDumpDetail(bool detail) noexcept {
    try {
        auto& log = state();
        std::lock_guard<std::mutex> lock(log.mutex);
        log.detail = detail;
    } catch (...) { /* Diagnostics must not abort emulation. */
    }
}
void LogDumpDetail(const char* format, ...) noexcept {
    std::va_list args;
    va_start(args, format);
    try {
        write_message(true, format, args);
    } catch (...) { /* Best-effort diagnostic output. */
    }
    va_end(args);
}
void LogDump(const char* format, ...) noexcept {
    std::va_list args;
    va_start(args, format);
    try {
        write_message(false, format, args);
    } catch (...) { /* Best-effort diagnostic output. */
    }
    va_end(args);
}
