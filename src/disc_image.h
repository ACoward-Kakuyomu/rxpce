#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rxpce_cd {
enum class TrackMode { audio, mode1_2352, mode1_2048 };
struct Track {
    unsigned number = 0;
    TrackMode mode = TrackMode::audio;
    std::size_t file = 0;
    // CUE: source BIN sector. CHD: track-relative stored sector (starts at 0).
    std::uint64_t file_sector = 0;
    std::int64_t pregap_lba = 0, file_lba = 0, start_lba = 0, end_lba = 0;
    unsigned sector_bytes() const {
        return mode == TrackMode::mode1_2048 ? 2048 : 2352;
    }
};
struct Toc {
    std::vector<Track> tracks;
    std::int64_t leadout = 0;
};
struct ImageFileInfo {
    std::uint64_t bytes = 0;
    std::string sha256;
};
struct SectorLocation {
    std::size_t track = 0;
    unsigned index = 0;
    bool generated = false;
    std::uint64_t file_sector = 0;
};
using ImageProgress = std::function<bool(std::uint64_t completed, std::uint64_t total)>;

bool msf_to_lba(const std::array<unsigned char, 3>& bcd, std::int64_t& lba);
bool lba_to_msf(std::int64_t lba, std::array<unsigned char, 3>& bcd);

// Single-owner media object. Calls are serialized on its owner's thread.
class DiscImage {
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    DiscImage();
    ~DiscImage();
    DiscImage(DiscImage&&) noexcept;
    DiscImage& operator=(DiscImage&&) noexcept;
    DiscImage(const DiscImage&) = delete;
    DiscImage& operator=(const DiscImage&) = delete;
    bool open_cue(const std::wstring& path, std::string& error, const ImageProgress& progress = {});
    bool open_chd(const std::wstring& path, std::string& error, const ImageProgress& progress = {});
    // Conversion helper: original CUE bytes plus generated gaps, in a new folder.
    bool write_materialized_cue(const std::wstring& folder, std::string& error,
                                const ImageProgress& progress = {});
    const Toc& toc() const;
    const std::vector<ImageFileInfo>& files() const;
    const std::string& identity() const;
    bool locate(std::int64_t lba, SectorLocation& position) const;
    bool read_data(std::int64_t lba, std::array<unsigned char, 2048>& output, std::string& error);
    bool read_audio(std::int64_t lba, std::array<std::int16_t, 1176>& output, std::string& error);
};
} // namespace rxpce_cd
