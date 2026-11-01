#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "log.h"
#include "file_paths.h"
#include "async_log.h"
#include "file_log.h"
#include <memory>
#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <atomic>
#include <optional>
#include <string_view>
#include <stdexcept>
#include <string>

namespace {
struct LogState;
void write_file(LogState& log, std::string_view text);
struct LogState {
    std::atomic_bool detail{true};
    bool rotation_attempted = false;
    std::filesystem::path path = rxpce_paths::executable().parent_path() / L"rxpce.log";
    std::unique_ptr<rxpce_log::FileWriter<>> file;
    rxpce_log::AsyncWriter writer;
    LogState() : writer([this](std::string_view text) { write_file(*this, text); }) {}
};
LogState& state() {
    static LogState value;
    return value;
}
std::optional<std::string> format_message(const char* format, std::va_list args) {
    if (!format)
        return std::string{};
    std::va_list count_args;
    va_copy(count_args, args);
    const int count = std::vsnprintf(nullptr, 0, format, count_args);
    va_end(count_args);
    if (count < 0 || static_cast<std::size_t>(count) > rxpce_log::AsyncWriter::byte_limit)
        return std::nullopt;
    std::string text(static_cast<std::size_t>(count) + 1, '\0');
    std::va_list write_args;
    va_copy(write_args, args);
    const int written = std::vsnprintf(text.data(), text.size(), format, write_args);
    va_end(write_args);
    if (written != count)
        return std::nullopt;
    text.resize(static_cast<std::size_t>(count));
    return text;
}
void prepare(LogState& log) {
    std::error_code error;
    if (!log.rotation_attempted) {
        log.rotation_attempted = true;
        if (std::filesystem::exists(log.path, error)) {
            auto backup = log.path;
            backup += L".bak";
            std::filesystem::remove(backup, error);
            std::filesystem::rename(log.path, backup, error);
        }
    }
    FILETIME created{}, exited{}, kernel{}, user{};
    GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
    ULARGE_INTEGER stamp{};
    stamp.LowPart = created.dwLowDateTime;
    stamp.HighPart = created.dwHighDateTime;
    char header[128];
    const int count = std::snprintf(header, sizeof(header), "Log session: pid=%lu started=%llu\n",
                                    GetCurrentProcessId(), stamp.QuadPart);
    if (count <= 0 || static_cast<std::size_t>(count) >= sizeof(header))
        throw std::runtime_error("Cannot format diagnostic session");
    log.file = std::make_unique<rxpce_log::FileWriter<>>(log.path, std::string(header, count));
}
#ifdef _DEBUG
void debug_message(std::string_view text) {
    const int length = static_cast<int>(text.size());
    const int count =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), length, nullptr, 0);
    if (count > 0) {
        std::wstring wide(static_cast<std::size_t>(count), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), length, wide.data(), count);
        OutputDebugStringW(wide.c_str());
    } else {
        const std::string narrow(text);
        OutputDebugStringA(narrow.c_str());
    }
}
#endif
void write_file(LogState& log, std::string_view text) {
    if (!log.file)
        prepare(log);
    (*log.file)(text);
#ifdef _DEBUG
    debug_message(text);
#endif
}
void write_message(bool detail, const char* format, std::va_list args) {
    auto& log = state();
    if (detail && !log.detail.load(std::memory_order_relaxed))
        return;
    auto text = format_message(format, args);
    if (text)
        log.writer.append(std::move(*text));
    else
        log.writer.note_drop();
}
} // namespace
void SetLogDumpDetail(bool detail) noexcept {
    try {
        state().detail.store(detail, std::memory_order_relaxed);
    } catch (...) { /* Diagnostics must not abort emulation. */
    }
}
void FlushLogDump() noexcept {
    try {
        state().writer.flush();
    } catch (...) { /* Best-effort diagnostics must not abort the application. */
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
