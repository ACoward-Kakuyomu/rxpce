#include "archive.h"
#include <miniz.h>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace rxpce_rom {
namespace {
std::mutex pending_mutex;
Cartridge pending;
bool occupied = false;
std::string lower(std::string text) {
    for (auto& c : text)
        if (c >= 'A' && c <= 'Z')
            c += 'a' - 'A';
    return text;
}
bool ends_with(const std::string& name, const char* suffix) {
    const std::string ending(suffix);
    return name.size() >= ending.size() &&
           lower(name.substr(name.size() - ending.size())) == ending;
}
std::vector<unsigned char> read_file(const std::filesystem::path& path, std::size_t limit) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        throw std::runtime_error("Cannot open ROM/ZIP file");
    const auto length = file.tellg();
    if (length < 0 || static_cast<unsigned long long>(length) > limit)
        throw std::runtime_error("ROM/ZIP file exceeds the supported size");
    std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), bytes.size()))
        throw std::runtime_error("Cannot read ROM/ZIP file");
    return bytes;
}
struct ZipReader {
    mz_zip_archive reader = {};
    bool opened = false;
    ~ZipReader() {
        if (opened)
            mz_zip_reader_end(&reader);
    }
};
void unzip(const std::vector<unsigned char>& archive, Cartridge& result) {
    ZipReader zip;
    zip.opened = mz_zip_reader_init_mem(&zip.reader, archive.data(), archive.size(),
                                        MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY) != 0;
    if (!zip.opened)
        throw std::runtime_error("Invalid ZIP archive");
    for (mz_uint index = 0; index < mz_zip_reader_get_num_files(&zip.reader); ++index) {
        if (mz_zip_reader_is_file_a_directory(&zip.reader, index))
            continue;
        const auto length = mz_zip_reader_get_filename(&zip.reader, index, nullptr, 0);
        if (!length)
            throw std::runtime_error("Invalid ZIP member name");
        std::string name(length, '\0');
        if (mz_zip_reader_get_filename(&zip.reader, index, &name[0], length) != length)
            throw std::runtime_error("Cannot read ZIP member name");
        name.resize(length - 1);
        const auto separator = name.find_last_of("/\\");
        const auto base = separator == std::string::npos ? 0 : separator + 1;
        if (name.compare(base, 2, "._") == 0 || name.rfind("__MACOSX/", 0) == 0 ||
            name.rfind("__MACOSX\\", 0) == 0)
            continue;
        if (!ends_with(name, ".pce"))
            continue;
        if (name.find('\0') != std::string::npos)
            throw std::runtime_error("Invalid ZIP member name");
        mz_zip_archive_file_stat info;
        if (!mz_zip_reader_file_stat(&zip.reader, index, &info))
            throw std::runtime_error("Invalid ZIP member");
        if (mz_zip_reader_is_file_encrypted(&zip.reader, index))
            throw std::runtime_error("Encrypted ZIP is not supported");
        if (info.m_uncomp_size < 8192 || info.m_uncomp_size > max_rom_bytes)
            throw std::runtime_error("Invalid ROM member size");
        result.bytes.resize(static_cast<std::size_t>(info.m_uncomp_size));
        if (!mz_zip_reader_extract_to_mem(&zip.reader, index, result.bytes.data(),
                                          result.bytes.size(), 0))
            throw std::runtime_error("Cannot decompress ZIP member or CRC mismatch");
        result.member_name = std::move(name);
        return;
    }
    throw std::runtime_error("ZIP contains no .PCE file");
}
} // namespace
bool load(const std::filesystem::path& path, Cartridge& result, std::string& error) {
    try {
        Cartridge next;
        next.source_path = path;
        const bool zipped = ends_with(path.filename().u8string(), ".zip");
        auto bytes = read_file(path, zipped ? 256u * 1024 * 1024 : max_rom_bytes);
        if (zipped)
            unzip(bytes, next);
        else
            next.bytes = std::move(bytes);
        const auto prefix = next.bytes.size() % 8192;
        if (next.bytes.size() - prefix < 8192)
            throw std::runtime_error("ROM must contain at least one 8 KiB bank");
        next.bytes.erase(next.bytes.begin(), next.bytes.begin() + prefix);
        next.profile = identify(next.bytes);
        result = std::move(next);
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
std::filesystem::path state_path(const std::filesystem::path& source,
                                 const std::filesystem::path& folder, rxpce_video::VdcMode mode) {
    auto name = source.filename();
    name.replace_extension(rxpce_video::state_extension(mode));
    const auto directory = folder.empty() || folder == L"." ? source.parent_path() : folder;
    return directory / name;
}
std::filesystem::path ram_path(const std::filesystem::path& source,
                               const std::filesystem::path& folder) {
    auto path = state_path(source, folder);
    path.replace_extension(L".ram");
    return path;
}
void queue(Cartridge image) {
    std::lock_guard<std::mutex> lock(pending_mutex);
    pending = std::move(image);
    occupied = true;
}
bool take(Cartridge& result) {
    std::lock_guard<std::mutex> lock(pending_mutex);
    if (!occupied)
        return false;
    result = std::move(pending);
    occupied = false;
    return true;
}
bool has_pending() {
    std::lock_guard<std::mutex> lock(pending_mutex);
    return occupied;
}
} // namespace rxpce_rom
