#include "disc_image_internal.h"
#include "disc_file_internal.h"
#include <libchdr/chd.h>
extern "C" {
#include <libchdr/cdrom.h>
}
#include <algorithm>
#include <limits>
#include <sstream>
#include <cstring>
#include <cstdio>
#include <set>
#include <utility>

namespace rxpce_cd {
using namespace detail;
namespace {
std::uint64_t big_endian(const unsigned char* bytes, unsigned count) {
    std::uint64_t result = 0;
    for (unsigned n = 0; n < count; ++n)
        result = (result << 8) | bytes[n];
    return result;
}
struct ChdHandle {
    chd_file* value = nullptr;
    ~ChdHandle() {
        if (value)
            chd_close(value);
    }
};
// Storage positions belong to this backend, never to the normalized TOC.
struct ChdStorage {
    File file;
    ChdHandle handle; // Destroy the decoder before its borrowed File.
    std::uint64_t position = 0, logical_bytes = 0;
    unsigned hunk_bytes = 0, replacement = 0;
    std::vector<std::uint64_t> track_frames;
    struct Cache {
        bool valid = false;
        unsigned number = 0;
        std::vector<std::uint16_t> words;
    };
    std::array<Cache, 2> cache;
    explicit ChdStorage(const std::wstring& path) : file(path) {}
    static std::uint64_t size(void* p) {
        return static_cast<ChdStorage*>(p)->file.bytes;
    }
    static int close_borrowed(void*) {
        return 0;
    }
    static int seek(void* p, std::int64_t offset, int origin) {
        auto& self = *static_cast<ChdStorage*>(p);
        const auto base = origin == SEEK_SET   ? 0
                          : origin == SEEK_CUR ? self.position
                                               : self.file.bytes;
        if (origin != SEEK_SET && origin != SEEK_CUR && origin != SEEK_END)
            return -1;
        if (offset < 0) {
            const auto distance = static_cast<std::uint64_t>(-(offset + 1)) + 1;
            if (distance > base)
                return -1;
            self.position = base - distance;
        } else {
            if (base > self.file.bytes ||
                static_cast<std::uint64_t>(offset) > self.file.bytes - base)
                return -1;
            self.position = base + offset;
        }
        return 0;
    }
    static std::size_t read(void* output, std::size_t size, std::size_t count, void* p) noexcept {
        auto& self = *static_cast<ChdStorage*>(p);
        if (!size || !count || count > std::numeric_limits<std::size_t>::max() / size)
            return 0;
        const auto bytes =
            (std::min)(static_cast<std::uint64_t>(size * count), self.file.bytes - self.position);
        std::uint64_t done = 0;
        try {
            while (done < bytes) {
                const auto chunk =
                    static_cast<unsigned>((std::min)(bytes - done, std::uint64_t(1024 * 1024)));
                self.file.read(self.position, static_cast<unsigned char*>(output) + done, chunk);
                self.position += chunk;
                done += chunk;
            }
        } catch (...) {
            return static_cast<std::size_t>(done / size);
        }
        return static_cast<std::size_t>(done / size);
    }
    static TrackMode mode(const std::string& text) {
        if (text == "AUDIO")
            return TrackMode::audio;
        if (text == "MODE1_RAW")
            return TrackMode::mode1_2352;
        if (text == "MODE1")
            return TrackMode::mode1_2048;
        throw std::runtime_error("Unsupported CHD track type: " + text);
    }
    static unsigned decimal(const std::string& text, unsigned limit) {
        if (text.empty())
            throw std::runtime_error("Missing CHD number");
        unsigned v = 0;
        for (auto ch : text) {
            if (ch < '0' || ch > '9' || v > limit / 10 || v * 10 + ch - '0' > limit)
                throw std::runtime_error("Invalid CHD number");
            v = v * 10 + ch - '0';
        }
        return v;
    }
    void metadata(std::uint64_t offset, Toc& toc) {
        struct Raw {
            bool present = false, recorded = false;
            TrackMode mode = TrackMode::audio;
            unsigned frames = 0, gap = 0;
        };
        std::array<Raw, 99> tracks{};
        std::set<std::uint64_t> visited;
        std::uint64_t total = 0;
        while (offset) {
            if (offset < 124 || !visited.insert(offset).second || visited.size() > 1024)
                throw std::runtime_error("Invalid or cyclic CHD metadata");
            std::array<unsigned char, 16> header{};
            file.read(offset, header.data(), 16);
            const auto length = big_endian(header.data() + 5, 3);
            total += length + 16;
            if (total > 1024 * 1024 || offset > file.bytes - 16 ||
                length > file.bytes - offset - 16)
                throw std::runtime_error("CHD metadata exceeds limits");
            const auto tag = big_endian(header.data(), 4);
            if (tag == CDROM_TRACK_METADATA2_TAG) {
                if (!length || length > 512)
                    throw std::runtime_error("Invalid CHD track metadata length");
                std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
                file.read(offset + 16, bytes.data(), static_cast<unsigned>(length));
                if (bytes.back() == 0)
                    bytes.pop_back();
                if (std::any_of(bytes.begin(), bytes.end(),
                                [](unsigned char v) { return v < 32 || v > 126; }))
                    throw std::runtime_error("Invalid CHD track metadata text");
                std::istringstream input(std::string(bytes.begin(), bytes.end()));
                const char* keys[] = {"TRACK:",  "TYPE:",   "SUBTYPE:", "FRAMES:",
                                      "PREGAP:", "PGTYPE:", "PGSUB:",   "POSTGAP:"};
                std::array<std::string, 8> values;
                for (unsigned n = 0; n < 8; ++n) {
                    std::string token;
                    if (!(input >> token) || token.compare(0, std::strlen(keys[n]), keys[n]) != 0)
                        throw std::runtime_error("Invalid CHD CHT2 fields");
                    values[n] = token.substr(std::strlen(keys[n]));
                }
                std::string extra;
                if (input >> extra)
                    throw std::runtime_error("Unexpected CHD CHT2 field");
                const auto number = decimal(values[0], 99);
                if (!number || tracks[number - 1].present)
                    throw std::runtime_error("Duplicate or invalid CHD track number");
                if (values[2] != "NONE" || values[6] != "NONE" || decimal(values[7], 900000) != 0)
                    throw std::runtime_error("Unsupported CHD subcode or postgap");
                auto& track = tracks[number - 1];
                track.present = true;
                track.mode = mode(values[1]);
                track.frames = decimal(values[3], 900000);
                track.gap = decimal(values[4], 900000);
                track.recorded = !values[5].empty() && values[5][0] == 'V';
                const auto pgmode = mode(track.recorded ? values[5].substr(1) : values[5]);
                if (!track.frames ||
                    (track.recorded && (track.gap >= track.frames || pgmode != track.mode)))
                    throw std::runtime_error("Invalid or unsupported CHD recorded pregap");
            } else {
                // Only CHT2 describes supported media. Do not silently ignore
                // alternate disc/session geometries mixed into an image.
                if (tag == CDROM_TRACK_METADATA_TAG || tag == GDROM_OLD_METADATA_TAG ||
                    tag == GDROM_TRACK_METADATA_TAG || (header[0] == 'C' && header[1] == 'H'))
                    throw std::runtime_error("Unsupported CHD disc/session metadata");
            }
            offset = big_endian(header.data() + 8, 8);
        }
        std::int64_t cursor = 0;
        std::uint64_t stored = 0;
        bool ended = false, data = false;
        for (unsigned n = 0; n < 99; ++n) {
            const auto& raw = tracks[n];
            if (!raw.present) {
                ended = true;
                continue;
            }
            if (ended)
                throw std::runtime_error("Nonsequential CHD tracks");
            Track track;
            track.number = n + 1;
            track.mode = raw.mode;
            track.pregap_lba = cursor;
            track.file_lba = cursor + (raw.recorded ? 0 : raw.gap);
            track.start_lba = cursor + raw.gap;
            track.end_lba = track.file_lba + raw.frames;
            // Public file_sector is track-relative for CHD, not a BIN offset.
            track.file_sector = 0;
            track_frames.push_back(stored);
            stored += (static_cast<std::uint64_t>(raw.frames) + 3) / 4 * 4;
            if (stored > logical_bytes / 2448)
                throw std::runtime_error("CHD tracks exceed logical stream");
            toc.tracks.push_back(track);
            cursor = track.end_lba;
            data |= track.mode != TrackMode::audio;
        }
        if (toc.tracks.empty() || !data || stored != logical_bytes / 2448)
            throw std::runtime_error("CHD CD track coverage mismatch");
        const auto origin = toc.tracks.front().start_lba;
        for (auto& track : toc.tracks) {
            track.pregap_lba -= origin;
            track.file_lba -= origin;
            track.start_lba -= origin;
            track.end_lba -= origin;
        }
        toc.leadout = cursor - origin;
        std::array<unsigned char, 3> end{};
        if (!lba_to_msf(toc.leadout, end))
            throw std::runtime_error("CHD leadout exceeds BCD MSF range");
    }
    void open(Toc& toc) {
        std::array<unsigned char, 124> header{};
        file.read(0, header.data(), 124);
        if (std::memcmp(header.data(), "MComprHD", 8) || big_endian(header.data() + 8, 4) != 124 ||
            big_endian(header.data() + 12, 4) != 5)
            throw std::runtime_error("Expected CHD v5 header");
        if (std::any_of(header.begin() + 104, header.end(), [](unsigned char v) { return v != 0; }))
            throw std::runtime_error("Parent CHD is unsupported");
        logical_bytes = big_endian(header.data() + 32, 8);
        hunk_bytes = static_cast<unsigned>(big_endian(header.data() + 56, 4));
        if (!logical_bytes || logical_bytes % 2448 || logical_bytes / 2448 > 900000 ||
            big_endian(header.data() + 60, 4) != 2448 || !hunk_bytes || hunk_bytes % 2448 ||
            hunk_bytes > 1024 * 1024)
            throw std::runtime_error("Invalid CHD CD size or hunk limits");
        for (unsigned n = 0; n < 4; ++n) {
            const auto codec = big_endian(header.data() + 16 + n * 4, 4);
            switch (codec) {
            case CHD_CODEC_NONE:
            case CHD_CODEC_ZLIB:
            case CHD_CODEC_LZMA:
            case CHD_CODEC_HUFFMAN:
            case CHD_CODEC_FLAC:
            case CHD_CODEC_ZSTD:
            case CHD_CODEC_CD_ZLIB:
            case CHD_CODEC_CD_LZMA:
            case CHD_CODEC_CD_FLAC:
            case CHD_CODEC_CD_ZSTD:
                break;
            default:
                throw std::runtime_error("Unsupported CHD codec");
            }
        }
        const auto map = big_endian(header.data() + 40, 8),
                   hunks = (logical_bytes + hunk_bytes - 1) / hunk_bytes;
        if (map < 124 || map > file.bytes ||
            (big_endian(header.data() + 16, 4) == 0 ? hunks * 4 : 16) > file.bytes - map)
            throw std::runtime_error("Invalid CHD map range");
        if (big_endian(header.data() + 16, 4) != 0) {
            std::array<unsigned char, 16> mapheader{};
            file.read(map, mapheader.data(), 16);
            const auto bytes = big_endian(mapheader.data(), 4);
            if (!bytes || bytes > hunks * 16 + 1024 || bytes > file.bytes - map - 16 ||
                mapheader[12] > 24 || mapheader[13] > 32 || mapheader[14] > 32)
                throw std::runtime_error("Invalid CHD compressed map limits");
        }
        metadata(big_endian(header.data() + 48, 8), toc);
        static const core_file_callbacks callbacks = {size, read, close_borrowed, seek};
        const auto error =
            chd_open_core_file_callbacks(&callbacks, this, CHD_OPEN_READ, nullptr, &handle.value);
        if (error != CHDERR_NONE)
            throw std::runtime_error(std::string("CHD open: ") + chd_error_string(error));
        const auto* decoded = chd_get_header(handle.value);
        // Bound libchdr's recursive self references before the first chd_read.
        // chdman writes backwards references; forward references are deferred.
        std::vector<unsigned char> depth(static_cast<std::size_t>(hunks), 0);
        for (unsigned n = 0; n < hunks; ++n) {
            const auto* entry = decoded->rawmap + n * decoded->mapentrybytes;
            if (decoded->mapentrybytes == 4) {
                const auto offset = big_endian(entry, 4) * hunk_bytes;
                if (offset &&
                    (offset < 124 || offset > file.bytes || hunk_bytes > file.bytes - offset))
                    throw std::runtime_error("Invalid CHD hunk range");
            } else {
                const auto offset = big_endian(entry + 4, 6), length = big_endian(entry + 1, 3);
                if (entry[0] <= 4) {
                    if ((entry[0] < 4 && decoded->compression[entry[0]] == 0) || !length ||
                        length > hunk_bytes || (entry[0] == 4 && length != hunk_bytes) ||
                        offset < 124 || offset > file.bytes || length > file.bytes - offset)
                        throw std::runtime_error("Invalid CHD compressed hunk range");
                } else if (entry[0] == 5) {
                    if (offset >= n || depth[static_cast<std::size_t>(offset)] >= 64)
                        throw std::runtime_error("Unsupported CHD self-reference chain");
                    depth[n] = depth[static_cast<std::size_t>(offset)] + 1;
                } else
                    throw std::runtime_error("Unsupported CHD parent or hunk type");
            }
        }
        for (auto& entry : cache)
            entry.words.resize(hunk_bytes / 2);
    }
    const unsigned char* hunk(unsigned number) {
        for (const auto& entry : cache)
            if (entry.valid && entry.number == number)
                return reinterpret_cast<const unsigned char*>(entry.words.data());
        auto& entry = cache[replacement++ % cache.size()];
        entry.valid = false;
        const auto error = chd_read(handle.value, number, entry.words.data());
        if (error != CHDERR_NONE)
            throw std::runtime_error(std::string("CHD hunk ") + std::to_string(number) + ": " +
                                     chd_error_string(error));
        entry.number = number;
        entry.valid = true;
        return reinterpret_cast<const unsigned char*>(entry.words.data());
    }
    const unsigned char* frame(std::size_t track, std::uint64_t sector) {
        const auto offset = (track_frames[track] + sector) * 2448;
        if (offset >= logical_bytes)
            throw std::runtime_error("Read outside CHD logical stream");
        return hunk(static_cast<unsigned>(offset / hunk_bytes)) + offset % hunk_bytes;
    }
};
} // namespace
struct DiscImage::Impl::Chd final : Impl {
    Chd() : Impl(Format::chd) {}
    std::unique_ptr<ChdStorage> chd;
    const unsigned char* read(std::size_t track, std::uint64_t sector) override {
        return chd->frame(track, sector);
    }
};
std::unique_ptr<DiscImage::Impl> DiscImage::Impl::open_chd(const std::wstring& path,
                                                           const ImageProgress& progress) {
    auto next = std::make_unique<Chd>();
    next->chd = std::make_unique<ChdStorage>(full_path(path));
    auto& chd = *next->chd;
    chd.open(next->toc);
    std::uint64_t done = 0;
    const auto report = [&] {
        if (progress && !progress(done, chd.logical_bytes))
            throw std::runtime_error("Disc image loading cancelled");
    };
    report();
    Sha256 stream;
    for (unsigned n = 0; done < chd.logical_bytes; ++n) {
        const auto count = static_cast<unsigned>(
            (std::min)(std::uint64_t(chd.hunk_bytes), chd.logical_bytes - done));
        stream.add(chd.hunk(n), count);
        done += count;
        report();
    }
    const auto sha = stream.finish();
    next->files.push_back({chd.file.bytes, sha});
    Sha256 identity;
    const char domain[] = "RXPCE-CHD-01";
    identity.add(domain, sizeof(domain) - 1);
    identity.integer(next->toc.tracks.size());
    for (const auto& track : next->toc.tracks) {
        identity.integer(track.number);
        identity.integer(static_cast<unsigned>(track.mode));
        identity.integer(static_cast<std::uint64_t>(track.pregap_lba));
        identity.integer(static_cast<std::uint64_t>(track.file_lba));
        identity.integer(static_cast<std::uint64_t>(track.start_lba));
        identity.integer(static_cast<std::uint64_t>(track.end_lba));
    }
    identity.integer(next->toc.leadout);
    identity.integer(chd.logical_bytes);
    identity.add(sha.data(), sha.size());
    next->identity = identity.finish();
    return next;
}
} // namespace rxpce_cd
