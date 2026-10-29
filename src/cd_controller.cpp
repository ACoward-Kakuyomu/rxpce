#include "cd_controller.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace rxpce_cd {
namespace {
unsigned char bcd(unsigned value) {
    return static_cast<unsigned char>((value / 10) * 16 + value % 10);
}
bool decode(unsigned char value, unsigned& result) {
    if ((value & 15) > 9 || (value >> 4) > 9)
        return false;
    result = (value >> 4) * 10 + (value & 15);
    return true;
}
void schedule(std::uint64_t& due, unsigned& fraction, std::uint64_t numerator,
              unsigned denominator) {
    due += numerator / denominator;
    fraction += static_cast<unsigned>(numerator % denominator);
    if (fraction >= denominator) {
        fraction -= denominator;
        ++due;
    }
}
} // namespace
void Controller::connect(const Toc* toc, ReadData read, ReadAudio audio) {
    toc_ = toc;
    read_data_ = std::move(read);
    read_audio_ = std::move(audio);
    adpcm_ram_.fill(0);
}
void Controller::configure_audio(unsigned rate) {
    if (rate > 192000)
        throw std::invalid_argument("Invalid CD audio rate");
    output_rate_ = rate;
    samples_.reset((rate + 4) / 5 * 2);
    output_due_ = output_previous_ = now_;
    output_fraction_ = rate ? rate - 1 : 0;
    area_left_ = area_right_ = 0;
    generated_frames_ = consumed_frames_ = 0;
    if (rate)
        schedule(output_due_, output_fraction_, clock_, rate);
}
void Controller::read_audio(float* out, std::size_t frames, unsigned channels) {
    if (channels == 2) {
        const auto available = samples_.queued() / 2;
        const auto missing = generated_frames_ - consumed_frames_ - available;
        const auto gap = static_cast<std::size_t>((std::min)(std::uint64_t(frames), missing));
        std::fill_n(out, gap * 2, 0.0f);
        samples_.read(out + gap * 2, (frames - gap) * 2);
        consumed_frames_ += gap + (std::min)(frames - gap, available);
    } else if (channels == 1) {
        float stereo[512];
        while (frames) {
            const auto chunk = (std::min)(frames, std::size_t(256));
            read_audio(stereo, chunk, 2);
            for (std::size_t n = 0; n < chunk; ++n)
                *out++ = (stereo[2 * n] + stereo[2 * n + 1]) * 0.5f;
            frames -= chunk;
        }
    } else
        throw std::invalid_argument("Invalid CD audio channels");
}
void Controller::reset_adpcm() {
    adpcm_playing_ = adpcm_half_ = adpcm_end_ = adpcm_low_ = false;
    adpcm_length_ = 0;
    adpcm_sample_ = 0;
    decoder_.reset();
    adpcm_latch_ = adpcm_read_ = adpcm_write_ = 0;
    adpcm_control_ = dma_control_ = 0;
    adpcm_read_due_ = adpcm_write_due_ = dma_busy_until_ = 0;
    adpcm_read_buffer_ = adpcm_write_value_ = 0;
    update_adpcm_irq();
}
void Controller::update_adpcm_irq() {
    irqs_ =
        static_cast<unsigned char>((irqs_ & ~12) | (adpcm_half_ ? 4 : 0) | (adpcm_end_ ? 8 : 0));
}
void Controller::length_read(bool port) {
    adpcm_half_ = adpcm_length_ < 32768;
    if (!(adpcm_control_ & 0x10)) {
        if (adpcm_length_)
            --adpcm_length_;
        else {
            if (port || adpcm_end_)
                adpcm_half_ = false;
            adpcm_end_ = true;
            if (adpcm_control_ & 0x40) {
                adpcm_playing_ = false;
                adpcm_sample_ = 0;
            }
        }
    }
    update_adpcm_irq();
}
void Controller::length_write() {
    adpcm_half_ = adpcm_length_ < 32768;
    if (!(adpcm_control_ & 0x10) && adpcm_length_ < 65535)
        ++adpcm_length_;
    update_adpcm_irq();
}
void Controller::tick_adpcm() {
    if (!adpcm_low_) {
        length_read(false);
        adpcm_byte_ = adpcm_ram_[adpcm_read_++];
    }
    if (adpcm_playing_) {
        const int sample = decoder_.decode((adpcm_byte_ >> (adpcm_low_ ? 0 : 4)) & 15);
        adpcm_sample_ = (((sample + 2048) & ~3) - 2048) / 2048.0f;
        if (!adpcm_pcm_logged_ && adpcm_sample_ != 0) {
            adpcm_pcm_logged_ = true;
            if (trace)
                trace("ADPCM decoded nonzero PCM");
        }
        adpcm_low_ = !adpcm_low_;
        schedule(adpcm_due_, adpcm_fraction_, std::uint64_t(clock_) * (16 - adpcm_rate_), 32000);
    }
}
void Controller::load_audio_sample() {
    cdda_sample_ = {};
    if (audio_state_ != 0 || !toc_)
        return;
    const auto track = std::find_if(toc_->tracks.begin(), toc_->tracks.end(), [&](const Track& t) {
        return current_lba_ >= t.pregap_lba && current_lba_ < t.end_lba;
    });
    if (track == toc_->tracks.end() || track->mode != TrackMode::audio || !read_audio_)
        return;
    if (cached_lba_ != current_lba_) {
        std::string problem;
        if (!read_audio_(current_lba_, pcm_, problem)) {
            audio_state_ = 3;
            error_ = problem.empty() ? "Cannot read CD audio" : problem;
            if (trace)
                trace("CD error: " + error_);
            return;
        }
        cached_lba_ = current_lba_;
    }
    cdda_sample_ = {{pcm_[audio_offset_ * 2], pcm_[audio_offset_ * 2 + 1]}};
    if (!cdda_pcm_logged_ && (cdda_sample_[0] || cdda_sample_[1])) {
        cdda_pcm_logged_ = true;
        if (trace)
            trace("CD-DA decoded nonzero PCM at LBA=" + std::to_string(current_lba_));
    }
}
void Controller::tick_cdda() {
    if (++audio_offset_ == 588) {
        audio_offset_ = 0;
        ++current_lba_;
        if (current_lba_ >= audio_end_) {
            if (audio_mode_ == 1 && audio_end_ > audio_start_)
                current_lba_ = audio_start_;
            else {
                audio_state_ = 3;
                if (audio_mode_ == 2)
                    irqs_ |= 0x20;
            }
        }
    }
    load_audio_sample();
    schedule(cdda_due_, cdda_fraction_, clock_, 44100);
}
void Controller::integrate(std::uint64_t until) {
    const auto elapsed = until - now_;
    const double decrease =
        (fade_control_ & 8) ? double(elapsed) / clock_ / ((fade_control_ & 4) ? 2.5 : 6.0) : 0;
    const double next = (std::max)(0.0, fade_volume_ - decrease);
    double average = (fade_volume_ + next) * 0.5;
    if (decrease > fade_volume_ && decrease > 0)
        average = fade_volume_ * fade_volume_ / (2 * decrease);
    const double cdda_gain = (fade_control_ & 2) ? 1 : average;
    const double adpcm_gain = (fade_control_ & 2) ? average : 1;
    if (output_rate_) {
        area_left_ +=
            (cdda_sample_[0] / 32768.0 * cdda_gain + adpcm_sample_ * adpcm_gain) * elapsed;
        area_right_ +=
            (cdda_sample_[1] / 32768.0 * cdda_gain + adpcm_sample_ * adpcm_gain) * elapsed;
    }
    fade_volume_ = next;
    now_ = until;
}
void Controller::emit_sample() {
    const double width = double(now_ - output_previous_);
    float pair[] = {width ? static_cast<float>(area_left_ / width) : 0,
                    width ? static_cast<float>(area_right_ / width) : 0};
    unsigned position = 0;
    samples_.append(2, [&](float* out, std::size_t count) {
        std::copy_n(pair + position, count, out);
        position += static_cast<unsigned>(count);
    });
    ++generated_frames_;
    area_left_ = area_right_ = 0;
    output_previous_ = now_;
    schedule(output_due_, output_fraction_, clock_, output_rate_);
}
void Controller::reset(unsigned clock) {
    if (clock < 75)
        throw std::invalid_argument("Invalid CD clock");
    clock_ = clock;
    now_ = due_ = 0;
    remainder_ = 0;
    phase_ = Phase::free;
    irqs_ = mask_ = latch_ = select_ = reset_ = speaker_ = sense_ = adpcm_control_ = 0;
    req_ = ack_ = ack_pending_ = motor_ = false;
    size_ = position_ = remaining_ = command_size_ = command_length_ = 0;
    next_lba_ = current_lba_ = audio_start_ = audio_end_ = 0;
    audio_mode_ = 0;
    audio_state_ = 3;
    error_.clear();
    adpcm_latch_ = adpcm_read_ = adpcm_write_ = 0;
    adpcm_read_buffer_ = adpcm_write_value_ = dma_control_ = 0;
    adpcm_read_due_ = adpcm_write_due_ = dma_due_ = dma_busy_until_ = 0;
    reset_adpcm();
    adpcm_rate_ = 0;
    cdda_sample_ = {};
    pcm_latch_ = {};
    cached_lba_ = -1;
    audio_offset_ = 0;
    cdda_pcm_logged_ = adpcm_pcm_logged_ = false;
    cdda_due_ = adpcm_due_ = 0;
    cdda_fraction_ = 44099;
    adpcm_fraction_ = 31999;
    fade_control_ = 0;
    fade_volume_ = 1;
    configure_audio(output_rate_);
}
void Controller::next_sector() {
    due_ += clock_ / 75;
    remainder_ += clock_ % 75;
    if (remainder_) {
        if (remainder_ >= 75) {
            remainder_ -= 75;
            ++due_;
        }
    }
}
void Controller::advance(std::uint64_t now) {
    if (now < now_)
        return; // Reset is explicit; no host queue may rewind this clock.
    for (;;) {
        auto next = now;
        auto event = [&](std::uint64_t due) { next = (std::min)(next, (std::max)(now_, due)); };
        if (output_rate_)
            event(output_due_);
        if (audio_state_ == 0)
            event(cdda_due_);
        if (adpcm_playing_)
            event(adpcm_due_);
        if (adpcm_write_due_)
            event(adpcm_write_due_);
        if (adpcm_read_due_)
            event(adpcm_read_due_);
        if (phase_ == Phase::waiting)
            event(due_);
        if ((dma_control_ & 3) && phase_ == Phase::data && req_ && !ack_)
            event(dma_due_);
        integrate(next);
        if (output_rate_ && now_ >= output_due_)
            emit_sample();
        if (adpcm_write_due_ && now_ >= adpcm_write_due_) {
            adpcm_ram_[adpcm_write_++] = adpcm_write_value_;
            adpcm_write_due_ = 0;
            length_write();
        }
        if (adpcm_read_due_ && now_ >= adpcm_read_due_) {
            adpcm_read_buffer_ = adpcm_ram_[adpcm_read_++];
            adpcm_read_due_ = 0;
            length_read(true);
        }
        if (phase_ == Phase::waiting && now_ >= due_) {
            std::string problem;
            if (!read_data_ || !read_data_(next_lba_, buffer_, problem)) {
                error(3, problem.empty() ? "Cannot read CD sector" : problem);
            } else {
                dma_due_ = (std::max)(dma_due_, due_);
                current_lba_ = next_lba_++;
                --remaining_;
                next_sector();
                size_ = 2048;
                position_ = 0;
                phase_ = Phase::data;
                latch_ = buffer_[0];
                req_ = !ack_;
                irqs_ |= 0x40;
            }
        } else if (phase_ == Phase::data && !ack_ && !ack_pending_)
            req_ = true;
        while ((dma_control_ & 3) && phase_ == Phase::data && req_ && !ack_ && now_ >= dma_due_) {
            adpcm_ram_[adpcm_write_++] = latch_;
            length_write();
            dma_busy_until_ = now_ + 11;
            dma_due_ = now_ + 10;
            ack(true);
            ack(false);
        }
        if (phase_ == Phase::status || phase_ == Phase::message || phase_ == Phase::free)
            dma_control_ &= ~3;
        if (adpcm_playing_ && now_ >= adpcm_due_)
            tick_adpcm();
        if (audio_state_ == 0 && now_ >= cdda_due_)
            tick_cdda();
        const bool pending =
            (output_rate_ && output_due_ <= now_) || (audio_state_ == 0 && cdda_due_ <= now_) ||
            (adpcm_playing_ && adpcm_due_ <= now_) || (phase_ == Phase::waiting && due_ <= now_) ||
            ((dma_control_ & 3) && phase_ == Phase::data && req_ && !ack_ && dma_due_ <= now_);
        if (now_ == now && !pending)
            break;
    }
}
void Controller::status(bool success) {
    phase_ = Phase::status;
    latch_ = success ? 0 : 1;
    req_ = !ack_;
    ack_pending_ = false;
    size_ = position_ = remaining_ = 0;
    command_size_ = 0;
}
void Controller::error(unsigned char sense, const std::string& message) {
    sense_ = sense;
    error_ = message;
    irqs_ &= ~0x40;
    if (trace)
        trace("CD error: " + message);
    status(false);
}
void Controller::reply(unsigned count) {
    size_ = count;
    position_ = 0;
    remaining_ = 0;
    phase_ = Phase::data;
    latch_ = buffer_[0];
    req_ = !ack_;
    irqs_ |= 0x40;
}
void Controller::consumed() {
    irqs_ &= ~0x40;
    if (remaining_) {
        phase_ = Phase::waiting;
        req_ = false;
    } else {
        irqs_ |= 0x20;
        status(true);
    }
}
void Controller::ack(bool value) {
    if (ack_ == value)
        return;
    ack_ = value;
    if (value && req_) {
        req_ = false;
        ack_pending_ = true;
        if (phase_ == Phase::command && command_size_ < command_.size())
            command_[command_size_++] = latch_;
        else if (phase_ == Phase::data)
            ++position_;
    } else if (!value) {
        if (!ack_pending_) {
            if (phase_ == Phase::command || phase_ == Phase::data || phase_ == Phase::status ||
                phase_ == Phase::message)
                req_ = true;
            return;
        }
        ack_pending_ = false;
        switch (phase_) {
        case Phase::command:
            if (command_size_ == 1)
                command_length_ = command_[0] < 0x20 ? 6 : 10;
            if (command_size_ == command_length_)
                execute();
            else
                req_ = true;
            break;
        case Phase::data:
            if (position_ == size_)
                consumed();
            else {
                latch_ = buffer_[position_];
                req_ = true;
            }
            break;
        case Phase::status:
            phase_ = Phase::message;
            latch_ = 0;
            req_ = true;
            break;
        case Phase::message:
            phase_ = Phase::free;
            req_ = false;
            irqs_ &= ~0x20;
            break;
        default:
            break;
        }
    }
}
bool Controller::decode_position(std::int64_t& lba) const {
    switch (command_[9] & 0xc0) {
    case 0:
        lba = std::int64_t(command_[3]) * 65536 + std::int64_t(command_[4]) * 256 + command_[5];
        return true;
    case 0x40:
        return msf_to_lba({{command_[2], command_[3], command_[4]}}, lba);
    case 0x80: {
        unsigned number = 0;
        if (command_[2] == 0xaa) {
            lba = toc_->leadout;
            return true;
        }
        if (!decode(command_[2], number) || !number || number > toc_->tracks.size())
            return false;
        lba = toc_->tracks[number - 1].start_lba;
        return true;
    }
    default:
        return false;
    }
}
void Controller::execute() {
    if (trace) {
        std::string description = "CD command=" + std::to_string(command_[0]);
        if (command_[0] == 8)
            description +=
                " lba=" +
                std::to_string(((command_[1] & 31) << 16) | (command_[2] << 8) | command_[3]) +
                " sectors=" + std::to_string(command_[4] ? command_[4] : 256);
        trace(description);
    }
    if (command_[0] == 3) {
        buffer_.fill(0);
        buffer_[0] = 0x70;
        buffer_[2] = sense_;
        buffer_[7] = 10;
        const auto count = (std::min)(18u, static_cast<unsigned>(command_[4]));
        sense_ = 0;
        if (count)
            reply(count);
        else
            status(true);
        return;
    }
    if (!toc_ || toc_->tracks.empty()) {
        error(2, "No CD medium");
        return;
    }
    switch (command_[0]) {
    case 0:
        status(true);
        break;
    case 8: {
        const auto lba = ((command_[1] & 31) << 16) | (command_[2] << 8) | command_[3];
        const unsigned count = command_[4] ? command_[4] : 256;
        if (lba >= toc_->leadout || count > toc_->leadout - lba) {
            error(5, "CD read out of range");
            break;
        }
        for (auto at = std::int64_t(lba); at < lba + count; ++at) {
            const auto found =
                std::find_if(toc_->tracks.begin(), toc_->tracks.end(),
                             [&](const Track& t) { return at >= t.pregap_lba && at < t.end_lba; });
            if (found == toc_->tracks.end() || found->mode == TrackMode::audio) {
                error(5, "READ targets an audio track");
                return;
            }
        }
        const bool sequential =
            motor_ && audio_state_ == 3 && lba == next_lba_ && next_lba_ == current_lba_ + 1;
        audio_state_ = 3;
        cdda_sample_ = {};
        motor_ = true;
        remaining_ = count;
        next_lba_ = lba;
        due_ = now_ + (lba == current_lba_ || sequential ? 0 : clock_ / 100);
        remainder_ = 74;
        next_sector();
        phase_ = Phase::waiting;
        req_ = false;
        irqs_ &= ~0x60;
        break;
    }
    case 0xde: {
        if (command_[1] == 0) {
            buffer_[0] = 1;
            buffer_[1] = bcd(static_cast<unsigned>(toc_->tracks.size()));
            reply(2);
        } else if (command_[1] == 1 || command_[1] == 2) {
            auto lba = toc_->leadout;
            unsigned char type = 4;
            if (command_[1] == 2 && command_[2] != 0xaa) {
                unsigned number = 0;
                if (!decode(command_[2], number) || !number || number > toc_->tracks.size()) {
                    error(5, "Invalid TOC track");
                    break;
                }
                const auto& track = toc_->tracks[number - 1];
                lba = track.start_lba;
                type = track.mode == TrackMode::audio ? 0 : 4;
            }
            std::array<unsigned char, 3> msf;
            if (!lba_to_msf(lba, msf)) {
                error(5, "TOC position out of range");
                break;
            }
            std::copy(msf.begin(), msf.end(), buffer_.begin());
            buffer_[3] = type;
            reply(command_[1] == 1 ? 3 : 4);
        } else
            error(5, "Invalid TOC request");
        break;
    }
    case 0xd8:
    case 0xd9: {
        std::int64_t lba = 0;
        if (!decode_position(lba) || lba < 0 || lba > toc_->leadout) {
            error(5, "Invalid audio position");
            break;
        }
        if (command_[0] == 0xd8) {
            audio_start_ = current_lba_ = lba;
            audio_end_ = toc_->leadout;
            audio_state_ = command_[1] ? 0 : 2;
            audio_mode_ = 3;
            audio_offset_ = 0;
            cached_lba_ = -1;
            cdda_pcm_logged_ = false;
        } else {
            audio_end_ = lba;
            audio_mode_ = command_[1] & 3;
            audio_state_ = audio_mode_ && lba > current_lba_ ? 0 : 3;
        }
        if (current_lba_ >= audio_end_)
            audio_state_ = 3;
        cdda_due_ = now_;
        cdda_fraction_ = 44099;
        schedule(cdda_due_, cdda_fraction_, clock_, 44100);
        load_audio_sample();
        motor_ = true;
        status(true);
        break;
    }
    case 0xda:
        if (audio_state_ == 3)
            error(5, "CD audio is not active");
        else {
            audio_state_ = 2;
            cdda_sample_ = {};
            status(true);
        }
        break;
    case 0xdd: {
        const auto found =
            std::find_if(toc_->tracks.begin(), toc_->tracks.end(), [&](const Track& track) {
                return current_lba_ >= track.pregap_lba && current_lba_ < track.end_lba;
            });
        const auto& track = found == toc_->tracks.end() ? toc_->tracks.back() : *found;
        const auto relative = current_lba_ - track.start_lba;
        std::array<unsigned char, 3> absolute{}, elapsed{};
        lba_to_msf(current_lba_, absolute);
        lba_to_msf((relative < 0 ? -relative : relative) - 150, elapsed);
        buffer_[0] = static_cast<unsigned char>(audio_state_);
        buffer_[1] = track.mode == TrackMode::audio ? 1 : 0x41;
        buffer_[2] = bcd(track.number);
        buffer_[3] = current_lba_ < track.start_lba ? 0 : 1;
        std::copy(elapsed.begin(), elapsed.end(), buffer_.begin() + 4);
        std::copy(absolute.begin(), absolute.end(), buffer_.begin() + 7);
        reply(10);
        break;
    }
    default:
        error(5, "Unsupported CD command " + std::to_string(command_[0]));
        break;
    }
}
unsigned char Controller::read(unsigned reg, std::uint64_t now) {
    advance(now);
    switch (reg & 15) {
    case 0: {
        unsigned char value = select_ & 7;
        if (phase_ != Phase::free)
            value |= 0x80;
        if (req_)
            value |= 0x40;
        if (phase_ == Phase::message)
            value |= 0x20;
        if (phase_ == Phase::command || phase_ == Phase::status || phase_ == Phase::message)
            value |= 0x10;
        if (phase_ != Phase::free && phase_ != Phase::command)
            value |= 8;
        return value;
    }
    case 1:
        return latch_;
    case 2:
        return mask_;
    case 3: {
        const auto value = (irqs_ & 0x6c) | (motor_ ? 0x10 : 0) | speaker_;
        speaker_ ^= 2;
        return static_cast<unsigned char>(value);
    }
    case 4:
        return reset_;
    case 8: {
        const auto value = latch_;
        if (phase_ == Phase::data && req_ && !ack_) {
            ack(true);
            ack(false);
        }
        return value;
    }
    case 0xa: {
        const auto value = adpcm_read_buffer_;
        adpcm_read_due_ = now_ + 19;
        return value;
    }
    case 0xb:
        return dma_control_;
    case 0xc:
        return static_cast<unsigned char>((adpcm_read_due_ ? 0x80 : 0) |
                                          (adpcm_write_due_ || now_ < dma_busy_until_ ? 4 : 0) |
                                          (adpcm_playing_ ? 8 : 0) | (adpcm_end_ ? 1 : 0));
    case 5:
        return static_cast<unsigned char>(pcm_latch_[(speaker_ & 2) ? 1 : 0]);
    case 6:
        return static_cast<unsigned char>(pcm_latch_[(speaker_ & 2) ? 1 : 0] >> 8);
    case 0xd:
        return adpcm_control_;
    default:
        return 0xff;
    }
}
void Controller::write(unsigned reg, unsigned char value, std::uint64_t now) {
    advance(now);
    switch (reg & 15) {
    case 0:
        select_ = value;
        irqs_ &= ~0x60;
        if (phase_ == Phase::free && !(reset_ & 2)) {
            phase_ = Phase::command;
            command_size_ = command_length_ = 0;
            req_ = !ack_;
            ack_pending_ = false;
        }
        break;
    case 1:
        latch_ = value;
        break;
    case 2:
        mask_ = value;
        ack((value & 0x80) != 0);
        break;
    case 4:
        if (value & 2) {
            phase_ = Phase::free;
            req_ = ack_ = ack_pending_ = false;
            irqs_ &= 0x0c;
            remaining_ = 0;
            audio_state_ = 3;
            cdda_sample_ = {};
            motor_ = false;
        }
        reset_ = value;
        break;
    case 5:
    case 6:
        for (unsigned ch = 0; ch < 2; ++ch)
            pcm_latch_[ch] =
                static_cast<std::uint16_t>(std::abs(static_cast<int>(cdda_sample_[ch])) *
                                           ((fade_control_ & 2) ? 1 : fade_volume_));
        break;
    case 8:
        adpcm_latch_ = static_cast<std::uint16_t>((adpcm_latch_ & 0xff00) | value);
        if (adpcm_control_ & 0x10)
            adpcm_length_ = adpcm_latch_;
        break;
    case 9:
        adpcm_latch_ =
            static_cast<std::uint16_t>((adpcm_latch_ & 255) | (static_cast<unsigned>(value) << 8));
        if (adpcm_control_ & 0x10)
            adpcm_length_ = adpcm_latch_;
        break;
    case 0xa:
        adpcm_write_value_ = value;
        adpcm_write_due_ = now_ + 11;
        break;
    case 0xb:
        dma_control_ = value;
        dma_due_ = now_;
        break;
    case 0xd:
        if (value & 0x80) {
            reset_adpcm();
        } else {
            if (!(value & 0x20)) {
                adpcm_playing_ = false;
                adpcm_sample_ = 0;
            } else if (!adpcm_playing_) {
                adpcm_playing_ = true;
                adpcm_low_ = false;
                adpcm_half_ = false;
                decoder_.reset();
                adpcm_sample_ = 0;
                adpcm_pcm_logged_ = false;
                adpcm_due_ = now_;
                adpcm_fraction_ = 31999;
                schedule(adpcm_due_, adpcm_fraction_, std::uint64_t(clock_) * (16 - adpcm_rate_),
                         32000);
                if (trace)
                    trace("ADPCM play: bytes=" + std::to_string(adpcm_length_) +
                          " rate=" + std::to_string(32000.0 / (16 - adpcm_rate_)));
            }
            if (value & 0x10) {
                adpcm_length_ = adpcm_latch_;
                adpcm_end_ = false;
            }
            if ((value & 8) && !(adpcm_control_ & 8))
                adpcm_read_ = static_cast<std::uint16_t>(adpcm_latch_ - ((value & 4) ? 0 : 1));
            if ((value & 2) && !(adpcm_control_ & 2))
                adpcm_write_ = static_cast<std::uint16_t>(adpcm_latch_ - ((value & 1) ? 0 : 1));
            adpcm_control_ = value;
            update_adpcm_irq();
        }
        break;
    case 0xe:
        adpcm_rate_ = value & 15;
        break;
    case 0xf:
        fade_control_ = value;
        if (!(value & 8))
            fade_volume_ = 1;
        break;
    default:
        break;
    }
}
} // namespace rxpce_cd
