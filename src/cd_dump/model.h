#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <vector>

namespace rxpce_cd_dump {
using Bytes = std::vector<std::uint8_t>;
class ScsiFailure : public std::runtime_error {
    unsigned status_;
    Bytes sense_;
  public:
    ScsiFailure(unsigned status,Bytes sense);
    bool mode_mismatch() const;
};
struct Track {
    unsigned number = 0, control = 0;
    std::int32_t index1 = 0;
    bool data() const { return (control & 4) != 0; }
};
struct Toc {
    std::vector<Track> tracks;
    std::int32_t leadout = 0;
};
struct Position {
    unsigned track = 0, index = 0, control = 0;
    std::int32_t absolute = 0, relative = 0;
};
// SCSI READ TOC format 0 with MSF=0; lengths include only returned bytes.
Toc parse_toc(const Bytes& bytes);
bool same_toc(const Toc& a, const Toc& b);
void validate_single_session(const Bytes& bytes);
// READ CD Q-only result: twelve Q bytes followed by four reserved bytes.
Position parse_q(const Bytes& bytes);
void validate_q(const Bytes& bytes);
unsigned gap_read_type(const Bytes& q,std::int32_t requested);
Bytes decode_raw_q(const Bytes& subchannels);
std::uint16_t q_crc(const std::uint8_t* bytes);
std::array<std::uint8_t, 12> read_cd_command(std::int32_t lba, unsigned sectors,
                                          unsigned subchannel,unsigned expected_type);
std::string hex(const Bytes& bytes);
} // namespace rxpce_cd_dump
