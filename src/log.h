#pragma once

// Messages use UTF-8; file paths are resolved through native wide APIs.
void SetLogDumpDetail(bool detail) noexcept;
void LogDumpDetail(const char* format, ...) noexcept;
void LogDump(const char* format, ...) noexcept;
