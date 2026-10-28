#pragma once
#include "transport.h"
#include <functional>
namespace rxpce_cd_dump {
struct ObservedPosition {
    std::int32_t requested = 0;
    Position q;
};
struct CaptureTrack {
    Track toc;
    std::int32_t begin = 0, end = 0;
    std::vector<std::uint8_t> gap_types;
};
struct Layout {
    std::string boundary_source = "unspecified";
    std::optional<std::uint32_t> media_generation;
    Toc toc;
    std::vector<CaptureTrack> tracks;
    std::vector<ObservedPosition> evidence;
    unsigned nonposition_records = 0;
    unsigned q_retries = 0;
};
using CancelCheck = std::function<void()>;
Layout discover_layout(Transport& drive, const CancelCheck& cancel = {});
std::string make_cue(const Layout& layout);
} // namespace rxpce_cd_dump
