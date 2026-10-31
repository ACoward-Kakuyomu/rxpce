#include "cd_controller.h"
#include "state_codec.h"
#include "audio_render_state.h"
#include <memory>
#include <limits>

namespace rxpce_cd {
std::size_t Controller::state_bytes() {
    static const auto size = std::make_unique<Controller>()->save_state().size();
    return size;
}
std::vector<char> Controller::save_state() const {
    rxpce_state::Writer w;
    w.block("CDDEV002", 8);
    auto put = [&](std::uint64_t n, unsigned count) { w.number(n, count); };
    put(clock_, 4);
    put(now_, 8);
    put(due_, 8);
    put(remainder_, 1);
    for (auto v : {static_cast<unsigned>(phase_), unsigned(irqs_), unsigned(mask_),
                   unsigned(latch_), unsigned(select_), unsigned(reset_), unsigned(speaker_),
                   unsigned(sense_), unsigned(adpcm_control_)})
        put(v, 1);
    for (bool v : {req_, ack_, ack_pending_, motor_})
        put(v, 1);
    put(size_, 2);
    put(position_, 2);
    put(remaining_, 2);
    put(command_size_, 1);
    put(command_length_, 1);
    w.block(command_.data(), command_.size());
    w.block(buffer_.data(), buffer_.size());
    for (auto v : {next_lba_, current_lba_, audio_start_, audio_end_})
        put(static_cast<std::uint64_t>(v), 8);
    put(audio_mode_, 1);
    put(audio_state_, 1);
    w.block(adpcm_ram_.data(), adpcm_ram_.size());
    for (auto v : {adpcm_latch_, adpcm_read_, adpcm_write_})
        put(v, 2);
    for (auto v : {adpcm_read_buffer_, adpcm_write_value_, dma_control_})
        put(v, 1);
    for (auto v : {adpcm_read_due_, adpcm_write_due_, dma_due_, dma_busy_until_})
        put(v, 8);
    for (bool v : {adpcm_playing_, adpcm_half_, adpcm_end_, adpcm_low_})
        put(v, 1);
    put(adpcm_length_, 2);
    put(adpcm_rate_, 1);
    put(adpcm_byte_, 1);
    put(static_cast<std::uint32_t>(decoder_.prediction()), 4);
    put(decoder_.index(), 1);
    put(adpcm_due_, 8);
    put(cdda_due_, 8);
    put(adpcm_fraction_, 2);
    put(cdda_fraction_, 2);
    put(audio_offset_, 2);
    put(static_cast<std::uint64_t>(cached_lba_), 8);
    for (auto v : pcm_)
        put(static_cast<std::uint16_t>(v), 2);
    for (auto v : pcm_latch_)
        put(v, 2);
    put(fade_control_, 1);
    w.real(fade_volume_);
    put(fade_origin_, 8);
    w.real(fade_initial_);
    put(native_due_, 8);
    put(native_fraction_, 4);
    put(native_previous_, 8);
    w.real(native_left_);
    w.real(native_right_);
    put(output_rate_, 4);
    put(output_due_, 8);
    put(output_fraction_, 4);
    rxpce_audio::write_filter_state(w, cdda_render_.state());
    rxpce_audio::write_filter_state(w, adpcm_render_.state());
    put(fade_cursor_, 4);
    for (const auto& gain : fade_history_)
        for (double value : gain)
            w.real(value);
    return std::move(w.data);
}
bool Controller::load_state(const char* data, std::size_t size, std::uint64_t cpu_time,
                            std::string& error) {
    try {
        auto require = [](bool ok, const char* why) {
            if (!ok)
                throw std::runtime_error(why);
        };
        require(data && size == state_bytes(), "Invalid CD device state length");
        auto candidate = std::make_unique<Controller>(*this);
        auto& s = *candidate;
        const unsigned requested_rate = output_rate_;
        rxpce_state::Reader r(data, size);
        char tag[8];
        r.block(tag, 8);
        require(std::memcmp(tag, "CDDEV002", 8) == 0, "Invalid CD device state version");
        s.clock_ = static_cast<unsigned>(r.number(4));
        require(s.clock_ == clock_ && s.clock_ >= 75, "CD clock mismatch");
        s.now_ = r.number(8);
        s.due_ = r.number(8);
        s.remainder_ = static_cast<unsigned>(r.number(1));
        const auto phase = r.number(1);
        require(phase <= static_cast<unsigned>(Phase::message), "Invalid CD phase");
        s.phase_ = static_cast<Phase>(phase);
        for (auto p : {&s.irqs_, &s.mask_, &s.latch_, &s.select_, &s.reset_, &s.speaker_, &s.sense_,
                       &s.adpcm_control_})
            *p = static_cast<unsigned char>(r.number(1));
        for (auto p : {&s.req_, &s.ack_, &s.ack_pending_, &s.motor_})
            *p = r.boolean();
        s.size_ = static_cast<unsigned>(r.number(2));
        s.position_ = static_cast<unsigned>(r.number(2));
        s.remaining_ = static_cast<unsigned>(r.number(2));
        s.command_size_ = static_cast<unsigned>(r.number(1));
        s.command_length_ = static_cast<unsigned>(r.number(1));
        r.block(s.command_.data(), s.command_.size());
        r.block(s.buffer_.data(), s.buffer_.size());
        for (auto p : {&s.next_lba_, &s.current_lba_, &s.audio_start_, &s.audio_end_})
            *p = r.signed_number(8);
        s.audio_mode_ = static_cast<unsigned>(r.number(1));
        s.audio_state_ = static_cast<unsigned>(r.number(1));
        r.block(s.adpcm_ram_.data(), s.adpcm_ram_.size());
        for (auto p : {&s.adpcm_latch_, &s.adpcm_read_, &s.adpcm_write_})
            *p = static_cast<std::uint16_t>(r.number(2));
        for (auto p : {&s.adpcm_read_buffer_, &s.adpcm_write_value_, &s.dma_control_})
            *p = static_cast<unsigned char>(r.number(1));
        for (auto p : {&s.adpcm_read_due_, &s.adpcm_write_due_, &s.dma_due_, &s.dma_busy_until_})
            *p = r.number(8);
        for (auto p : {&s.adpcm_playing_, &s.adpcm_half_, &s.adpcm_end_, &s.adpcm_low_})
            *p = r.boolean();
        s.adpcm_length_ = static_cast<unsigned>(r.number(2));
        s.adpcm_rate_ = static_cast<unsigned>(r.number(1));
        s.adpcm_byte_ = static_cast<unsigned>(r.number(1));
        const auto prediction = r.signed_number(4);
        const auto index = r.number(1);
        require(prediction >= -2048 && prediction <= 2047 &&
                    s.decoder_.restore(static_cast<int>(prediction), static_cast<unsigned>(index)),
                "Invalid ADPCM decoder");
        s.adpcm_due_ = r.number(8);
        s.cdda_due_ = r.number(8);
        s.adpcm_fraction_ = static_cast<unsigned>(r.number(2));
        s.cdda_fraction_ = static_cast<unsigned>(r.number(2));
        s.audio_offset_ = static_cast<unsigned>(r.number(2));
        s.cached_lba_ = r.signed_number(8);
        for (auto& v : s.pcm_)
            v = static_cast<std::int16_t>(r.signed_number(2));
        for (auto& v : s.pcm_latch_)
            v = static_cast<std::uint16_t>(r.number(2));
        s.fade_control_ = static_cast<unsigned>(r.number(1));
        s.fade_volume_ = r.real();
        s.fade_origin_ = r.number(8);
        s.fade_initial_ = r.real();
        require(s.fade_origin_ <= s.now_ && std::isfinite(s.fade_initial_) &&
                    s.fade_initial_ >= 0 && s.fade_initial_ <= 1,
                "Invalid fade clock");
        const double expected_fade =
            (std::max)(0.0, s.fade_initial_ - ((s.fade_control_ & 8)
                                                   ? double(s.now_ - s.fade_origin_) / s.clock_ /
                                                         ((s.fade_control_ & 4) ? 2.5 : 6.0)
                                                   : 0));
        require(s.fade_volume_ == expected_fade, "CD fade value/clock mismatch");
        s.native_due_ = r.number(8);
        s.native_fraction_ = static_cast<unsigned>(r.number(4));
        s.native_previous_ = r.number(8);
        s.native_left_ = r.real();
        s.native_right_ = r.real();
        s.output_rate_ = static_cast<unsigned>(r.number(4));
        s.output_due_ = r.number(8);
        s.output_fraction_ = static_cast<unsigned>(r.number(4));
        rxpce_audio::BandlimitedBuffer::State cdda_filter, adpcm_filter;
        require(rxpce_audio::read_filter_state(r, cdda_filter) &&
                    rxpce_audio::read_filter_state(r, adpcm_filter),
                "Invalid CD filter state");
        s.fade_cursor_ = static_cast<unsigned>(r.number(4));
        for (auto& gain : s.fade_history_)
            for (auto& value : gain) {
                value = r.real();
                require(std::isfinite(value) && value >= 0 && value <= 1,
                        "Invalid CD gain history");
            }
        require(s.fade_cursor_ < s.fade_history_.size(), "Invalid CD gain cursor");
        require(s.output_rate_ == 0 || (s.output_rate_ >= 1000 && s.output_rate_ <= 192000 &&
                                        s.clock_ >= (std::max)(s.output_rate_, 44100u)),
                "Invalid CD output rate");
        if (s.output_rate_) {
            require(s.native_fraction_ < 44100 && s.output_fraction_ < s.output_rate_ &&
                        s.native_previous_ <= s.now_ && s.native_due_ > s.now_ &&
                        s.native_due_ - s.native_previous_ <=
                            (std::uint64_t(s.clock_) + 44099) / 44100 &&
                        s.output_due_ > s.now_ &&
                        s.output_due_ - s.now_ <=
                            (std::uint64_t(s.clock_) + s.output_rate_ - 1) / s.output_rate_,
                    "Invalid CD render clock");
            const double elapsed = double(s.now_ - s.native_previous_);
            require(std::isfinite(s.native_left_) && std::isfinite(s.native_right_) &&
                        std::fabs(s.native_left_) <= elapsed + 1e-8 &&
                        std::fabs(s.native_right_) <= elapsed + 1e-8,
                    "Invalid CD partial PCM");
            require(cdda_filter.rate == s.output_rate_ && cdda_filter.pcm_rate == 44100 &&
                        adpcm_filter.rate == s.output_rate_ && adpcm_filter.pcm_rate == 0 &&
                        s.cdda_render_.restore(cdda_filter) &&
                        s.adpcm_render_.restore(adpcm_filter),
                    "CD filter rate mismatch");
        } else {
            require(!cdda_filter.rate && !adpcm_filter.rate, "Inactive CD retained filters");
            s.cdda_render_ = {};
            s.adpcm_render_ = {};
        }
        require(!r.remaining && s.clock_ == clock_ && s.now_ == cpu_time,
                "CD clock or CPU timestamp mismatch");
        require(s.now_ < UINT64_MAX - std::uint64_t(clock_) * 2, "CD timestamp out of range");
        const auto limit = s.now_ + std::uint64_t(clock_) * 2;
        require(s.remainder_ < 75 && s.command_size_ <= 10 &&
                    (s.command_length_ == 0 || s.command_length_ == 6 || s.command_length_ == 10),
                "Invalid CD command state");
        require(s.command_size_ <= s.command_length_ ||
                    (s.command_length_ == 0 && s.command_size_ <= 1),
                "Invalid CD command position");
        require(s.size_ <= 2048 && s.position_ <= s.size_ && s.remaining_ <= 256,
                "Invalid CD transfer state");
        require(!(s.irqs_ & ~0x6c) && s.sense_ <= 15 && (s.speaker_ == 0 || s.speaker_ == 2) &&
                    !(s.ack_ && s.req_) && (!s.ack_pending_ || (s.ack_ && !s.req_)),
                "Invalid CD bus or IRQ state");
        require(s.phase_ != Phase::free || (!s.req_ && !s.ack_pending_), "Invalid free CD bus");
        require(s.phase_ != Phase::data || (s.size_ && (s.position_ < s.size_ || s.ack_pending_)),
                "Invalid CD data position");
        require(s.phase_ != Phase::waiting || (s.remaining_ && !s.req_ && s.due_ > s.now_),
                "Invalid CD transfer deadline");
        require(toc_ && !toc_->tracks.empty(), "CD medium is not connected");
        for (auto v : {s.next_lba_, s.current_lba_, s.audio_start_, s.audio_end_})
            require(v >= 0 && v <= toc_->leadout, "Invalid CD sector address");
        require(s.cached_lba_ >= -1 && s.cached_lba_ < toc_->leadout && s.audio_offset_ < 588 &&
                    s.audio_mode_ <= 3 &&
                    (s.audio_state_ == 0 || s.audio_state_ == 2 || s.audio_state_ == 3),
                "Invalid CD audio state");
        if (s.cached_lba_ >= 0) {
            const auto track =
                std::find_if(toc_->tracks.begin(), toc_->tracks.end(), [&](const Track& t) {
                    return s.cached_lba_ >= t.pregap_lba && s.cached_lba_ < t.end_lba;
                });
            require(track != toc_->tracks.end() && track->mode == TrackMode::audio,
                    "Invalid CD audio cache sector");
        }
        for (auto sample : s.pcm_latch_)
            require(sample <= 32768, "Invalid CD PCM latch");
        require(s.audio_state_ != 0 || (s.current_lba_ < s.audio_end_ && s.cdda_due_ > s.now_),
                "Invalid CD audio deadline");
        require(s.adpcm_rate_ <= 15 && s.adpcm_fraction_ < 32000 && s.cdda_fraction_ < 44100 &&
                    !(s.adpcm_control_ & 0x80),
                "Invalid ADPCM phase");
        require(s.adpcm_playing_ == ((s.adpcm_control_ & 0x20) != 0) ||
                    (!s.adpcm_playing_ && s.adpcm_end_ && (s.adpcm_control_ & 0x40)),
                "Invalid ADPCM playback flags");
        require((s.irqs_ & 12) == ((s.adpcm_half_ ? 4 : 0) | (s.adpcm_end_ ? 8 : 0)),
                "Invalid ADPCM IRQ state");
        for (auto v : {s.due_, s.dma_due_, s.dma_busy_until_, s.adpcm_due_, s.cdda_due_})
            require(v <= limit, "CD deadline too far ahead");
        require(!s.adpcm_playing_ || s.adpcm_due_ > s.now_, "Invalid ADPCM playback deadline");
        require(!s.adpcm_read_due_ ||
                    (s.adpcm_read_due_ > s.now_ && s.adpcm_read_due_ - s.now_ <= 19),
                "Invalid ADPCM read deadline");
        require(!s.adpcm_write_due_ ||
                    (s.adpcm_write_due_ > s.now_ && s.adpcm_write_due_ - s.now_ <= 11),
                "Invalid ADPCM write deadline");
        require(std::isfinite(s.fade_volume_) && s.fade_volume_ >= 0 && s.fade_volume_ <= 1,
                "Invalid CD fade volume");
        s.cdda_sample_ = {};
        if (s.audio_state_ == 0 && s.cached_lba_ == s.current_lba_)
            s.cdda_sample_ = {{s.pcm_[s.audio_offset_ * 2], s.pcm_[s.audio_offset_ * 2 + 1]}};
        s.adpcm_sample_ =
            s.adpcm_playing_ ? (((s.decoder_.prediction() + 2048) & ~3) - 2048) / 2048.0f : 0;
        s.error_.clear();
        s.cdda_pcm_logged_ = s.adpcm_pcm_logged_ = false;
        s.adpcm_render_level_ = s.adpcm_sample_;
        s.samples_.reset((s.output_rate_ + 4) / 5 * 2);
        s.generated_frames_ = s.consumed_frames_ = 0;
        s.audio_statistics_ = {};
        if (s.output_rate_ != requested_rate)
            s.configure_audio(requested_rate);
        *this = std::move(s);
        error.clear();
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
}
} // namespace rxpce_cd
