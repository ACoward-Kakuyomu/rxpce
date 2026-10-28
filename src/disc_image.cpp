#include "disc_image_internal.h"
#include "disc_sector_internal.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace rxpce_cd {
using detail::edc;
namespace {
bool bcd_byte(unsigned char value, unsigned limit) {
    const auto byte = static_cast<unsigned>(value);
    return (byte & 15) <= 9 && (byte >> 4) <= 9 && (byte >> 4) * 10 + (byte & 15) <= limit;
}
} // namespace

bool msf_to_lba(const std::array<unsigned char, 3>& bcd, std::int64_t& lba) {
    if (!bcd_byte(bcd[0], 99) || !bcd_byte(bcd[1], 59) || !bcd_byte(bcd[2], 74))
        return false;
    const auto decode = [](unsigned v) { return (v >> 4) * 10 + (v & 15); };
    lba = static_cast<std::int64_t>((decode(bcd[0]) * 60 + decode(bcd[1])) * 75 + decode(bcd[2])) -
          150;
    return true;
}
bool lba_to_msf(std::int64_t lba, std::array<unsigned char, 3>& bcd) {
    if (lba < -150 || lba > 449849)
        return false;
    const auto frames = lba + 150;
    const auto encode = [](unsigned v) {
        return static_cast<unsigned char>(((v / 10) << 4) | (v % 10));
    };
    bcd = {{encode(static_cast<unsigned>(frames / 4500)),
            encode(static_cast<unsigned>(frames / 75 % 60)),
            encode(static_cast<unsigned>(frames % 75))}};
    return true;
}
DiscImage::DiscImage() = default;
DiscImage::~DiscImage() = default;
DiscImage::DiscImage(DiscImage&&) noexcept = default;
DiscImage& DiscImage::operator=(DiscImage&&) noexcept = default;
const Toc& DiscImage::toc() const {
    static const Toc empty;
    return impl_ ? impl_->toc : empty;
}
const std::vector<ImageFileInfo>& DiscImage::files() const {
    static const std::vector<ImageFileInfo> empty;
    return impl_ ? impl_->files : empty;
}
const std::string& DiscImage::identity() const {
    static const std::string empty;
    return impl_ ? impl_->identity : empty;
}

bool DiscImage::open_cue(const std::wstring& path, std::string& error,
                         const ImageProgress& progress) {
    try {
        auto next = Impl::open_cue(path, progress);
        impl_ = std::move(next);
        error.clear();
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
}
bool DiscImage::open_chd(const std::wstring& path, std::string& error,
                         const ImageProgress& progress) {
    try {
        auto next = Impl::open_chd(path, progress);
        impl_ = std::move(next);
        error.clear();
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
}
bool DiscImage::locate(std::int64_t lba, SectorLocation& position) const {
    if (!impl_)
        return false;
    for (std::size_t n = 0; n < impl_->toc.tracks.size(); ++n) {
        const auto& track = impl_->toc.tracks[n];
        if (lba >= track.pregap_lba && lba < track.end_lba) {
            SectorLocation next;
            next.track = n;
            next.index = lba < track.start_lba ? 0 : 1;
            next.generated = lba < track.file_lba;
            next.file_sector =
                next.generated
                    ? 0
                    : track.file_sector + static_cast<std::uint64_t>(lba - track.file_lba);
            position = next;
            return true;
        }
    }
    return false;
}
bool DiscImage::read_data(std::int64_t lba, std::array<unsigned char, 2048>& output,
                          std::string& error) {
    try {
        SectorLocation location;
        if (!locate(lba, location))
            throw std::runtime_error("LBA outside disc");
        const auto& track = impl_->toc.tracks[location.track];
        if (track.mode == TrackMode::audio)
            throw std::runtime_error("AUDIO track is not a data sector");
        std::array<unsigned char, 2048> next{};
        if (!location.generated) {
            const auto bytes = impl_->read(location.track, location.file_sector);
            if (track.mode == TrackMode::mode1_2048)
                std::copy_n(bytes, 2048, next.begin());
            else {
                bool sync = bytes[0] == 0 && bytes[11] == 0;
                for (unsigned n = 1; n < 11; ++n)
                    sync &= bytes[n] == 0xff;
                std::uint32_t stored = 0;
                for (unsigned n = 0; n < 4; ++n)
                    stored |= static_cast<std::uint32_t>(bytes[2064 + n]) << (n * 8);
                bool reserved = true;
                for (unsigned n = 2068; n < 2076; ++n)
                    reserved &= bytes[n] == 0;
                if (!sync || bytes[15] != 1 || !bcd_byte(bytes[12], 99) ||
                    !bcd_byte(bytes[13], 59) || !bcd_byte(bytes[14], 74) || !reserved ||
                    edc(bytes, 2064) != stored)
                    throw std::runtime_error("Invalid Mode 1 sector header or EDC");
                std::copy_n(bytes + 16, 2048, next.begin());
            }
        }
        output = next;
        error.clear();
        return true;
    } catch (const std::exception& ex) {
        error = "LBA " + std::to_string(lba) + ": " + ex.what();
        return false;
    }
}
bool DiscImage::read_audio(std::int64_t lba, std::array<std::int16_t, 1176>& output,
                           std::string& error) {
    try {
        SectorLocation location;
        if (!locate(lba, location))
            throw std::runtime_error("LBA outside disc");
        const auto& track = impl_->toc.tracks[location.track];
        if (track.mode != TrackMode::audio)
            throw std::runtime_error("Data track is not an AUDIO sector");
        std::array<std::int16_t, 1176> next{};
        if (!location.generated) {
            const auto bytes = impl_->read(location.track, location.file_sector);
            for (unsigned n = 0; n < next.size(); ++n) {
                const int value = impl_->format == Impl::Format::chd
                                      ? (static_cast<int>(bytes[n * 2]) << 8) + bytes[n * 2 + 1]
                                      : bytes[n * 2] + (static_cast<int>(bytes[n * 2 + 1]) << 8);
                next[n] = static_cast<std::int16_t>(value < 32768 ? value : value - 65536);
            }
        }
        output = next;
        error.clear();
        return true;
    } catch (const std::exception& ex) {
        error = "LBA " + std::to_string(lba) + ": " + ex.what();
        return false;
    }
}
} // namespace rxpce_cd
