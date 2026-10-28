#pragma once
#include <cstddef>
#include <string>
namespace rxpce_storage {
bool load_nvram(const std::wstring& path, unsigned char* ram, std::size_t size, std::string& error);
bool save_nvram(const std::wstring& path, const unsigned char* ram, std::size_t size,
                std::string& error);
} // namespace rxpce_storage
