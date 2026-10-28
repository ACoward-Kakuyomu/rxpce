#pragma once
#include <filesystem>
#include <string>

namespace rxpce_paths {
std::filesystem::path executable();
inline std::filesystem::path from_utf8(const std::string& text) {
    return std::filesystem::u8path(text);
}
inline std::string utf8(const std::filesystem::path& path) {
    return path.u8string();
}
} // namespace rxpce_paths
