#pragma once
#include <cstdint>
namespace rxpce_video {
enum class VdcMode : std::uint8_t { accurate, fast };
constexpr bool valid_vdc_mode(VdcMode mode) noexcept {
    return mode == VdcMode::accurate || mode == VdcMode::fast;
}
constexpr const wchar_t* state_extension(VdcMode mode) noexcept {
    return mode == VdcMode::fast ? L".fst0" : L".st0";
}
} // namespace rxpce_video
