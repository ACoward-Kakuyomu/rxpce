#pragma once
#include "disc_image.h"
#include "adpcm_decoder.h"
#include "audio_mixer.h"
#include <functional>

namespace rxpce_cd {
class Controller {
  public:
    using ReadData =
        std::function<bool(std::int64_t, std::array<unsigned char, 2048>&, std::string&)>;
    using ReadAudio =
        std::function<bool(std::int64_t, std::array<std::int16_t, 1176>&, std::string&)>;
    enum class Phase { free, command, waiting, data, status, message };
    void connect(const Toc* toc, ReadData read, ReadAudio audio = {});
    void configure_audio(unsigned rate);
    void read_audio(float* out, std::size_t frames, unsigned channels);
    std::vector<char> save_state() const;
    bool load_state(const char* data, std::size_t size, std::uint64_t cpu_time, std::string& error);
    static std::size_t state_bytes();
    void reset(unsigned clock_hz);
    void advance(std::uint64_t now);
    unsigned char read(unsigned reg, std::uint64_t now);
    void write(unsigned reg, unsigned char value, std::uint64_t now);
    bool irq() const {
        return (irqs_ & mask_ & 0x6c) != 0;
    }
    const std::string& last_error() const {
        return error_;
    }
    Phase phase() const {
        return phase_;
    }
    std::function<void(const std::string&)> trace;

  private:
    const Toc* toc_ = nullptr;
    ReadData read_data_;
    ReadAudio read_audio_;
    Phase phase_ = Phase::free;
    unsigned clock_ = 7170000, remainder_ = 0;
    std::uint64_t now_ = 0, due_ = 0;
    unsigned char irqs_ = 0, mask_ = 0, latch_ = 0, select_ = 0, reset_ = 0, speaker_ = 0,
                  sense_ = 0, adpcm_control_ = 0;
    bool req_ = false, ack_ = false, ack_pending_ = false, motor_ = false;
    std::array<unsigned char, 10> command_{};
    unsigned command_size_ = 0, command_length_ = 0;
    std::array<unsigned char, 2048> buffer_{};
    unsigned size_ = 0, position_ = 0, remaining_ = 0;
    std::int64_t next_lba_ = 0, current_lba_ = 0, audio_start_ = 0, audio_end_ = 0;
    unsigned audio_mode_ = 0, audio_state_ = 3;
    std::string error_;
    std::array<unsigned char, 65536> adpcm_ram_{};
    std::uint16_t adpcm_latch_ = 0, adpcm_read_ = 0, adpcm_write_ = 0;
    unsigned char adpcm_read_buffer_ = 0, adpcm_write_value_ = 0, dma_control_ = 0;
    std::uint64_t adpcm_read_due_ = 0, adpcm_write_due_ = 0, dma_due_ = 0, dma_busy_until_ = 0;
    AdpcmDecoder decoder_;
    bool adpcm_playing_ = false, adpcm_half_ = false, adpcm_end_ = false, adpcm_low_ = false;
    unsigned adpcm_length_ = 0, adpcm_rate_ = 0, adpcm_byte_ = 0;
    float adpcm_sample_ = 0;
    std::uint64_t adpcm_due_ = 0, cdda_due_ = 0, output_due_ = 0;
    unsigned adpcm_fraction_ = 31999, cdda_fraction_ = 44099, output_fraction_ = 0;
    unsigned audio_offset_ = 0, output_rate_ = 0;
    std::int64_t cached_lba_ = -1;
    std::array<std::int16_t, 1176> pcm_{};
    std::array<std::int16_t, 2> cdda_sample_{};
    std::array<std::uint16_t, 2> pcm_latch_{};
    bool cdda_pcm_logged_ = false, adpcm_pcm_logged_ = false;
    rxpce_audio::SampleQueue samples_;
    std::uint64_t generated_frames_ = 0, consumed_frames_ = 0;
    double area_left_ = 0, area_right_ = 0;
    std::uint64_t output_previous_ = 0;
    unsigned fade_control_ = 0;
    double fade_volume_ = 1;
    void load_audio_sample();
    void tick_cdda();
    void tick_adpcm();
    void length_read(bool port);
    void length_write();
    void update_adpcm_irq();
    void reset_adpcm();
    void integrate(std::uint64_t until);
    void emit_sample();
    void ack(bool value);
    void execute();
    void status(bool success);
    void reply(unsigned count);
    void next_sector();
    void consumed();
    void error(unsigned char sense, const std::string& message);
    bool decode_position(std::int64_t& lba) const;
};
} // namespace rxpce_cd
