#include "capture.h"
#include "disc_sector_internal.h"
#include <algorithm>
#include <array>
namespace rxpce_cd_dump {
namespace {
unsigned char twice(unsigned value) {
    return static_cast<unsigned char>((value << 1) ^ ((value & 128) ? 0x11d : 0));
}
// Mode1 parity over GF(2^8), with polynomial x^8+x^4+x^3+x^2+1.
// Two parity symbols close each interleaved data stripe.
bool parity_matches(const unsigned char* data, const unsigned char* parity,
                    unsigned stripes, unsigned length, unsigned step, unsigned rowStep) {
    std::array<unsigned char,256> divideByThree{};
    for (unsigned n = 0; n < 256; ++n) divideByThree[n ^ twice(n)] = static_cast<unsigned char>(n);
    const auto size = stripes * length;
    for (unsigned stripe = 0; stripe < stripes; ++stripe) {
        auto address = (stripe / 2) * rowStep + stripe % 2;
        unsigned char sum = 0, weighted = 0;
        for (unsigned item = 0; item < length; ++item) {
            const auto value = data[address];
            sum ^= value;
            weighted = twice(weighted ^ value);
            address = (address + step) % size;
        }
        const auto first = divideByThree[twice(weighted) ^ sum];
        if (parity[stripe] != first || parity[stripe + stripes] != (first ^ sum)) return false;
    }
    return true;
}
unsigned char bcd(unsigned value) { return static_cast<unsigned char>((value / 10 << 4) | (value % 10)); }
}
namespace {
void validate_header(const std::uint8_t* s,std::int32_t lba) {
    if (lba < 0 || lba > 449849) throw std::runtime_error("Invalid MODE1 address");
    const auto time = static_cast<unsigned>(lba + 150);
    if (s[0] != 0 || s[11] != 0 || s[15] != 1 ||
        !std::all_of(s + 1, s + 11, [](auto b) { return b == 255; }) ||
        s[12] != bcd(time / 4500) || s[13] != bcd(time / 75 % 60) || s[14] != bcd(time % 75))
        throw std::runtime_error("Invalid MODE1 header/address for LBA " + std::to_string(lba) +
                                 ", header=" + hex(Bytes(s,s+16)));
}
}
void validate_mode1(const std::uint8_t* s, std::int32_t lba) {
    validate_header(s,lba);
    const auto stored = std::uint32_t(s[2064]) | (std::uint32_t(s[2065]) << 8) |
                        (std::uint32_t(s[2066]) << 16) | (std::uint32_t(s[2067]) << 24);
    if (rxpce_cd::detail::edc(s,2064) != stored ||
        !std::all_of(s + 2068, s + 2076, [](auto b) { return b == 0; }))
        throw std::runtime_error("Invalid MODE1 EDC or reserved bytes");
    if (!parity_matches(s + 12, s + 2076, 86,24,86,2) ||
        !parity_matches(s + 12, s + 2248, 52,43,88,86))
        throw std::runtime_error("Invalid MODE1 P/Q parity");
}
CaptureStats capture(Transport& drive, const Layout& layout, CaptureSink& output,
                     const CaptureOptions& options, const Progress& progress, const CancelCheck& cancel) {
    if (layout.tracks.empty() || layout.tracks.size() != layout.toc.tracks.size() ||
        layout.tracks.front().begin != 0 || options.batch == 0 || options.batch > 16 || options.retries > 10)
        throw std::runtime_error("Invalid capture layout or options");
    std::int32_t cursor = 0, lastAudio = 0;
    std::optional<std::int32_t> firstAudio;
    const auto markAudio = [&](std::int32_t address) {
        if (!firstAudio) firstAudio = address;
        lastAudio = address;
    };
    for (std::size_t n = 0; n < layout.tracks.size(); ++n) {
        const auto& track = layout.tracks[n];
        const auto& toc = layout.toc.tracks[n];
        if (track.begin != cursor || track.toc.number != toc.number || track.toc.control != toc.control ||
            track.toc.index1 != toc.index1 || track.begin > toc.index1 || track.end <= toc.index1 ||
            track.gap_types.size()!=static_cast<std::size_t>(toc.index1-track.begin) ||
            !std::all_of(track.gap_types.begin(),track.gap_types.end(),[](auto t){return t==1 || t==2;}))
            throw std::runtime_error("Capture layout is not contiguous or does not match the TOC");
        cursor = track.end;
        for (std::size_t gap = 0; gap < track.gap_types.size(); ++gap)
            if (track.gap_types[gap] == 1) markAudio(track.begin + static_cast<std::int32_t>(gap));
        if (!toc.data()) { markAudio(toc.index1); markAudio(track.end - 1); }
    }
    if (cursor != layout.toc.leadout) throw std::runtime_error("Capture layout does not reach leadout");
    if (firstAudio && *firstAudio == lastAudio)
        throw std::runtime_error("A distinct audio sector is required to establish the audio stream");
    const auto generation=drive.media_generation();
    if (layout.media_generation && layout.media_generation != generation)
        throw std::runtime_error("Media changed since layout discovery");
    const auto checkMedia = [&] {
        if (cancel) cancel();
        if (drive.media_generation()!=generation) throw std::runtime_error("Media generation changed during capture");
        if (!same_toc(read_toc(drive), layout.toc)) throw std::runtime_error("Media changed during capture");
    };
    CaptureStats stats;
    stats.media_generation_available=generation.has_value();
    stats.capture_batch=options.batch;
    stats.verify_batch=(std::max)(1u,options.batch-1);
    checkMedia();
    for (unsigned pass = 0; pass < 2; ++pass) {
        unsigned previousReadType = 0;
        const auto verifying = pass != 0;
        const auto phase=verifying?CapturePhase::verifying:CapturePhase::reading;
        if (progress) progress({verifying,0,0,static_cast<std::uint64_t>(cursor),phase});
        for (const auto& track : layout.tracks) {
            std::function<void(std::int32_t,unsigned)> block;
            block = [&](std::int32_t lba, unsigned count) {
                std::string failure;
                for (unsigned attempt = 0; attempt <= options.retries; ++attempt) {
                    if (cancel) cancel();
                    Bytes bytes;
                    bool valid = false;
                    try {
                        const auto type = lba >= track.toc.index1 ? (track.toc.data() ? 2u : 1u) :
                            track.gap_types.at(static_cast<std::size_t>(lba-track.begin));
                        if (type == 1 && previousReadType != 1) {
                            // Establish CD-DA decoding at a different known audio
                            // address, then return. Never shift or synthesize PCM.
                            const auto anchor = lba - *firstAudio > lastAudio - lba ? *firstAudio : lastAudio;
                            read_cd(drive,anchor,1,0,1);
                            ++stats.audio_prime_reads;
                            previousReadType = 1;
                            if (cancel) cancel();
                        }
                        bytes=read_cd(drive,lba,count,0,type);
                        previousReadType = type;
                        if (type == 2 && lba < track.toc.index1) validate_header(bytes.data(),lba);
                        for (unsigned n = 0; n < count; ++n) {
                            if (track.toc.data() && lba + static_cast<std::int32_t>(n) >= track.toc.index1)
                                validate_mode1(bytes.data() + n * 2352, lba + static_cast<std::int32_t>(n));
                        }
                        valid = true;
                    } catch (const Cancelled&) { throw; }
                    catch (const std::exception& e) { failure = e.what(); }
                    const auto offset = static_cast<std::uint64_t>(lba - track.begin);
                    if (valid && verifying && !output.matches(track.toc.number, offset, bytes)) {
                        valid = false;
                        failure = "Repeated read differs from the captured RAW bytes";
                    }
                    if (valid) {
                        if (!verifying) output.write(track.toc.number, offset, bytes);
                        auto& done = verifying ? stats.verified : stats.captured;
                        done += count;
                        if (progress) progress({verifying,track.toc.number,done,static_cast<std::uint64_t>(cursor),phase,lba});
                        return;
                    }
                    if (count > 1) {
                        ++stats.splits;
                        if(progress) progress({verifying,track.toc.number,verifying?stats.verified:stats.captured,
                            static_cast<std::uint64_t>(cursor),CapturePhase::retrying,lba,0});
                        const auto left = count / 2;
                        block(lba, left);
                        block(lba + static_cast<std::int32_t>(left), count - left);
                        return;
                    }
                    if (attempt != options.retries) {
                        ++stats.retries;
                        if(progress) progress({verifying,track.toc.number,verifying?stats.verified:stats.captured,
                            static_cast<std::uint64_t>(cursor),CapturePhase::retrying,lba,attempt+1});
                    }
                }
                throw std::runtime_error("Capture failed at LBA " + std::to_string(lba) + ": " + failure);
            };
            for (auto lba = track.begin; lba < track.end;) {
                // Gap types were determined from CRC-protected Q, not inferred
                // by treating data-read failures as permission to use audio.
                const auto count = lba<track.toc.index1?1u:
                    (std::min)(verifying?stats.verify_batch:stats.capture_batch,static_cast<unsigned>(track.end-lba));
                block(lba, count);
                lba += static_cast<std::int32_t>(count);
            }
        }
        checkMedia();
    }
    if (cancel) cancel();
    if(progress) progress({true,0,stats.verified,static_cast<std::uint64_t>(cursor),CapturePhase::finalizing});
    if (cancel) cancel();
    output.commit(layout, stats);
    return stats;
}
}
