#include "vdc_dma.h"
#include <algorithm>
#include <stdexcept>

namespace rxpce_video {
bool valid_dma(const VdcDmaState& s) noexcept {
    if (static_cast<unsigned>(s.phase) > static_cast<unsigned>(DmaPhase::write) ||
        s.remaining > 65536 || (s.direction & ~12) || s.elapsed >= 2 || s.satb_elapsed >= 4)
        return false;
    if (s.phase == DmaPhase::idle ? (s.remaining || s.elapsed || s.read_latch) : !s.remaining)
        return false;
    if (s.phase == DmaPhase::requested && (s.elapsed || s.read_latch))
        return false;
    if (s.phase == DmaPhase::read && s.read_latch)
        return false;
    return s.satb_running ? s.satb_index < 256 : (s.satb_index == 0 && s.satb_elapsed == 0);
}
void VdcDma::restore(const VdcDmaState& value) {
    if (!valid_dma(value))
        throw std::invalid_argument("Invalid VDC DMA state");
    state_ = value;
}
void VdcDma::request_vram(std::uint16_t source, std::uint16_t destination, std::uint16_t length,
                          std::uint16_t control) noexcept {
    state_.source = source;
    state_.destination = destination;
    state_.remaining = std::uint32_t(length) + 1;
    state_.direction = static_cast<std::uint8_t>(control & 12);
    state_.phase = DmaPhase::requested;
    state_.read_latch = 0;
    state_.elapsed = 0;
}
void VdcDma::request_satb(std::uint16_t source) noexcept {
    state_.satb_requested = true;
    state_.satb_request_source = source;
}
void VdcDma::vblank_start(bool repeat, std::uint16_t source) noexcept {
    if (!repeat && !state_.satb_requested)
        return;
    state_.satb_source = state_.satb_requested ? state_.satb_request_source : source;
    state_.satb_requested = false;
    state_.satb_running = true;
    state_.satb_index = 0;
    state_.satb_elapsed = 0;
}
void VdcDma::display_start(bool burst) noexcept {
    if (burst)
        return;
    state_.phase = DmaPhase::idle;
    state_.remaining = 0;
    state_.read_latch = 0;
    state_.elapsed = 0;
    state_.satb_running = false;
    state_.satb_index = 0;
    state_.satb_elapsed = 0;
}
void VdcDma::advance_to(std::uint64_t target, bool window, const DmaCallbacks& cb) {
    if (target < state_.dot_time)
        throw std::invalid_argument("DMA time moved backwards");
    if (!cb.read || !cb.write || !cb.satb_write || !cb.progress || !cb.complete)
        throw std::invalid_argument("Incomplete DMA callbacks");
    auto& s = state_;
    while (s.dot_time < target) {
        if (!window || !busy()) {
            s.dot_time = target;
            break;
        }
        if (s.satb_running) {
            const auto n = std::min<std::uint64_t>(target - s.dot_time, 4 - s.satb_elapsed);
            s.dot_time += n;
            s.satb_elapsed += static_cast<std::uint8_t>(n);
            if (s.satb_elapsed != 4)
                continue;
            s.satb_elapsed = 0;
            const auto value = cb.read(
                cb.context, static_cast<std::uint16_t>(s.satb_source + s.satb_index), s.dot_time);
            cb.satb_write(cb.context, s.satb_index, value, s.dot_time);
            if (++s.satb_index == 256) {
                s.satb_running = false;
                s.satb_index = 0;
                cb.complete(cb.context, DmaChannel::satb, s.dot_time);
            }
        } else {
            if (s.phase == DmaPhase::requested)
                s.phase = DmaPhase::read;
            const auto n = std::min<std::uint64_t>(target - s.dot_time, 2 - s.elapsed);
            s.dot_time += n;
            s.elapsed += static_cast<std::uint8_t>(n);
            if (s.elapsed != 2)
                continue;
            s.elapsed = 0;
            if (s.phase == DmaPhase::read) {
                s.read_latch = cb.read(cb.context, s.source, s.dot_time);
                s.phase = DmaPhase::write;
            } else {
                cb.write(cb.context, s.destination, s.read_latch, s.dot_time);
                s.source = static_cast<std::uint16_t>(s.source + ((s.direction & 4) ? -1 : 1));
                s.destination =
                    static_cast<std::uint16_t>(s.destination + ((s.direction & 8) ? -1 : 1));
                --s.remaining;
                s.read_latch = 0;
                s.phase = s.remaining ? DmaPhase::read : DmaPhase::idle;
                cb.progress(cb.context, s.source, s.destination,
                            static_cast<std::uint16_t>(s.remaining - 1));
                if (!s.remaining)
                    cb.complete(cb.context, DmaChannel::vram, s.dot_time);
            }
        }
    }
}
} // namespace rxpce_video
