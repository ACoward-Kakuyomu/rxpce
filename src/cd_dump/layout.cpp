#include "layout.h"
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
namespace rxpce_cd_dump {
namespace {
class Discovery {
    Transport& drive_;
    const CancelCheck& cancel_;
  public:
    Layout result;
    std::map<std::int32_t, Position> positions;
    Discovery(Transport& drive, const CancelCheck& cancel) : drive_(drive), cancel_(cancel) {}
    Bytes raw_q(std::int32_t request) {
        Bytes rawQ;
        for (unsigned attempt = 0;; ++attempt) {
            if (cancel_) cancel_();
            try {
                const auto bytes = read_q_frame(drive_,request);
                rawQ.assign(bytes.begin() + 2352, bytes.end());
                validate_q(rawQ);
                break;
            } catch (const std::exception& e) {
                if (attempt == 3)
                    throw std::runtime_error("Q at requested LBA " + std::to_string(request) + ": " + e.what());
                ++result.q_retries;
            }
        }
        return rawQ;
    }
    bool sample(std::int32_t request, Position& position) {
        const auto rawQ=raw_q(request);
        if ((rawQ[0] & 15) != 1) { ++result.nonposition_records; return false; }
        position = parse_q(rawQ);
        // The observed device returns Q two sectors behind main data in MODE1.
        // Never silently relabel Q as belonging to the requested address.
        if (std::abs(position.absolute - request) > 8 || position.absolute < -150 ||
            position.absolute >= result.toc.leadout || position.track > result.toc.tracks.size())
            throw std::runtime_error("Q address is inconsistent with the read request/TOC");
        if (position.index == 1 && position.absolute - position.relative !=
            result.toc.tracks[position.track - 1].index1)
            throw std::runtime_error("Q INDEX 01 disagrees with the TOC");
        const auto found = positions.find(position.absolute);
        if (found != positions.end() && (found->second.track != position.track || found->second.index != position.index))
            throw std::runtime_error("Conflicting Q positions");
        positions[position.absolute] = position;
        result.evidence.push_back({request, position});
        return true;
    }
    ObservedPosition nearby(std::int32_t request) {
        for (unsigned n = 0; n < 8 && request + static_cast<std::int32_t>(n) < result.toc.leadout; ++n) {
            Position p;
            if (sample(request + static_cast<std::int32_t>(n), p))
                return {request + static_cast<std::int32_t>(n), p};
        }
        throw std::runtime_error("No positional Q near the requested sector");
    }
    void scan(std::int32_t low, std::int32_t high) {
        for (auto request = (std::max)(low, 0); request <= high && request < result.toc.leadout; ++request) {
            Position p;
            sample(request, p);
        }
    }
};
std::string msf(std::int32_t frames) {
    if (frames < 0 || frames >= 450000) throw std::runtime_error("CUE index is outside MSF range");
    std::ostringstream out;
    out << std::setfill('0') << std::setw(2) << frames / 4500 << ':'
        << std::setw(2) << frames / 75 % 60 << ':' << std::setw(2) << frames % 75;
    return out.str();
}
}
Layout discover_layout(Transport& drive, const CancelCheck& cancel) {
    const auto generation = drive.media_generation();
    const auto profile = read_profile(drive);
    if (profile != 8 && profile != 9 && profile != 10) throw std::runtime_error("Only CD-ROM/CD-R/CD-RW media are supported");
    read_session(drive);
    Discovery discovery(drive, cancel);
    auto& result = discovery.result;
    result.boundary_source = "positional_q";
    result.media_generation = generation;
    result.toc = read_toc(drive);
    if (result.toc.tracks.front().index1 != 0) throw std::runtime_error("First track must start at program LBA 0");
    bool hasData = false;
    for (const auto& track : result.toc.tracks) {
        if ((track.control & ~4u) != 0) throw std::runtime_error("Unsupported track control flags");
        hasData |= track.data();
    }
    if (!hasData) throw std::runtime_error("A game data track is required");
    for (std::size_t n = 0; n < result.toc.tracks.size(); ++n) {
        const auto& track = result.toc.tracks[n];
        discovery.scan(track.index1 - 8, track.index1 + 8);
        const auto start = discovery.positions.find(track.index1);
        if (start == discovery.positions.end() || start->second.track != track.number || start->second.index != 1)
            throw std::runtime_error("Cannot confirm INDEX 01 using positional Q");
        std::int32_t begin = 0;
        if (n != 0) {
            auto low = result.toc.tracks[n - 1].index1;
            auto high = (std::min)(track.index1 + 8, result.toc.leadout - 1);
            if (discovery.nearby(low).q.track >= track.number ||
                discovery.nearby(high).q.track != track.number)
                throw std::runtime_error("Cannot bracket the track gap");
            while (high - low > 32) {
                const auto observation = discovery.nearby(low + (high - low) / 2);
                if (observation.q.track >= track.number) high = observation.requested;
                else low = observation.requested;
            }
            discovery.scan(low - 8, high + 8);
            bool found = false;
            for (const auto& entry : discovery.positions) {
                const auto& current = entry.second;
                if (current.track != track.number) continue;
                const auto before = discovery.positions.find(entry.first - 1);
                if (before == discovery.positions.end() || before->second.track != track.number - 1 ||
                    before->second.index != 1) continue;
                if (found || entry.first > track.index1 ||
                    (current.index == 1 && entry.first != track.index1))
                    throw std::runtime_error("Ambiguous track gap boundary");
                begin = entry.first;
                found = true;
            }
            if (!found) throw std::runtime_error("Cannot establish an adjacent Q boundary for INDEX 00 of track " +
                                                 std::to_string(track.number));
        }
        result.tracks.push_back({track, begin, result.toc.leadout});
        if (n != 0) result.tracks[n - 1].end = begin;
    }
    for (const auto& track : result.tracks) {
        if (track.begin > track.toc.index1 || track.end <= track.toc.index1)
            throw std::runtime_error("Invalid capture track range");
    }
    // Prefetch gap CONTROL fields in CD-DA mode before reading any MODE1 main
    // bytes. This avoids speculative MODE1 requests to audio pauses, which some
    // devices report as an uncorrectable-data error rather than wrong mode.
    for(auto& track:result.tracks) {
        for(auto lba=track.begin;lba<track.toc.index1;++lba) {
            const auto q=discovery.raw_q(lba);
            const auto type=gap_read_type(q,lba);
            if((q[0]&15)==1) {
                const auto position=parse_q(q);
                if(position.track!=track.toc.number || position.index!=0)
                    throw std::runtime_error("Gap Q index disagrees with discovered boundary");
                if(lba==track.begin || lba+1==track.toc.index1 ||
                   track.gap_types.empty() || track.gap_types.back()!=type)
                    result.evidence.push_back({lba,position});
            } else ++result.nonposition_records;
            track.gap_types.push_back(static_cast<std::uint8_t>(type));
        }
    }
    if (cancel) cancel();
    if (drive.media_generation() != generation)
        throw std::runtime_error("Media generation changed during layout discovery");
    if (!same_toc(read_toc(drive), result.toc))
        throw std::runtime_error("Media TOC changed during layout discovery");
    return std::move(result);
}
std::string make_cue(const Layout& layout) {
    std::ostringstream out;
    for (const auto& track : layout.tracks) {
        out << "FILE \"track" << std::setfill('0') << std::setw(2) << track.toc.number << ".bin\" BINARY\n"
            << "  TRACK " << std::setw(2) << track.toc.number << (track.toc.data() ? " MODE1/2352\n" : " AUDIO\n");
        if (track.begin < track.toc.index1) out << "    INDEX 00 00:00:00\n";
        out << "    INDEX 01 " << msf(track.toc.index1 - track.begin) << '\n';
    }
    return out.str();
}
}
