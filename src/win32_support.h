#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tchar.h>
#include <cstdio>
#include <cstdarg>
#ifdef _DEBUG
inline void rxpce_trace(const char* format, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, format);
    vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
    va_end(args);
    OutputDebugStringA(buffer);
}
#define TRACE(...) rxpce_trace(__VA_ARGS__)
#else
#define TRACE(...) ((void)0)
#endif
