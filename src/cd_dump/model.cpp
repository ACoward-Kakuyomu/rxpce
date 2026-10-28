#include "model.h"
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>
namespace rxpce_cd_dump {
bool same_toc(const Toc& a, const Toc& b) {
    if (a.leadout != b.leadout || a.tracks.size() != b.tracks.size()) return false;
    for (std::size_t n = 0; n < a.tracks.size(); ++n) {
        if (a.tracks[n].number != b.tracks[n].number || a.tracks[n].control != b.tracks[n].control ||
            a.tracks[n].index1 != b.tracks[n].index1) return false;
    }
    return true;
}

ScsiFailure::ScsiFailure(unsigned status,Bytes sense)
    : std::runtime_error("SCSI status "+std::to_string(status)+", sense="+hex(sense)),
      status_(status),sense_(std::move(sense)) {}
bool ScsiFailure::mode_mismatch() const {
    if(status_!=2 || sense_.empty()) return false;
    const auto format=sense_[0]&0x7f;
    if((format==0x70 || format==0x71) && sense_.size()>=14)
        return (sense_[2]&15)==5 && sense_[12]==0x64 && sense_[13]==0;
    if((format==0x72 || format==0x73) && sense_.size()>=4)
        return (sense_[1]&15)==5 && sense_[2]==0x64 && sense_[3]==0;
    return false;
}
namespace {
unsigned bcd(std::uint8_t value, unsigned maximum) {
    const auto result = unsigned(value >> 4) * 10 + (value & 15);
    if ((value >> 4) > 9 || (value & 15) > 9 || result > maximum)
        throw std::runtime_error("Invalid BCD in Q subchannel");
    return result;
}
std::int32_t msf(const std::uint8_t* bytes) {
    return static_cast<std::int32_t>((bcd(bytes[0], 99) * 60 + bcd(bytes[1], 59)) * 75 + bcd(bytes[2], 74));
}
unsigned declared_length(const Bytes& bytes) {
    if (bytes.size() < 4) throw std::runtime_error("Short TOC header");
    const auto length = unsigned(bytes[0]) * 256 + bytes[1] + 2;
    if (length < 4 || length > bytes.size()) throw std::runtime_error("Truncated TOC response");
    return length;
}
}
Toc parse_toc(const Bytes& bytes) {
    const auto length = declared_length(bytes);
    if (bytes[2] != 1 || bytes[3] < 1 || bytes[3] > 99 ||
        length != 4 + (unsigned(bytes[3]) + 1) * 8)
        throw std::runtime_error("Unsupported TOC track range or length");
    Toc toc;
    std::int32_t previous = -1;
    for (unsigned n = 0; n <= bytes[3]; ++n) {
        const auto* d = bytes.data() + 4 + n * 8;
        const auto number = n == bytes[3] ? 0xaa : n + 1;
        const auto address = (std::uint32_t(d[4]) << 24) | (std::uint32_t(d[5]) << 16) |
                             (std::uint32_t(d[6]) << 8) | d[7];
        if (d[2] != number || (d[1] >> 4) != 1 || address > 449849 ||
            static_cast<std::int32_t>(address) <= previous)
            throw std::runtime_error("Invalid TOC track or non-increasing address");
        previous = static_cast<std::int32_t>(address);
        if (n == bytes[3]) toc.leadout = previous;
        else toc.tracks.push_back({number, unsigned(d[1] & 15), previous});
    }
    return toc;
}
void validate_single_session(const Bytes& bytes) {
    if (declared_length(bytes) != 12 || bytes[2] != 1 || bytes[3] != 1 || bytes[6] != 1)
        throw std::runtime_error("Only a completed single-session disc is supported");
}
std::uint16_t q_crc(const std::uint8_t* bytes) {
    std::uint16_t crc = 0;
    for (unsigned n = 0; n < 10; ++n) {
        crc ^= std::uint16_t(bytes[n]) << 8;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = static_cast<std::uint16_t>((crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0));
    }
    return static_cast<std::uint16_t>(~crc);
}
void validate_q(const Bytes& bytes) {
    if (bytes.size() != 16) throw std::runtime_error("Short Q-only response");
    const auto crc = q_crc(bytes.data());
    if (crc != ((unsigned(bytes[10]) << 8) | bytes[11]))
        throw std::runtime_error("Invalid Q CRC");
}
unsigned gap_read_type(const Bytes& q,std::int32_t requested) {
    validate_q(q);
    if(requested<0 || requested>449849) throw std::runtime_error("Invalid gap address");
    const auto control=q[0]>>4;
    if((control&~4u)!=0) throw std::runtime_error("Unsupported gap control flags");
    const auto adr=q[0]&15;
    if(adr==1) {
        if(parse_q(q).absolute!=requested) throw std::runtime_error("Gap Q address differs from request");
    } else if(adr==2 || adr==3) {
        if(bcd(q[9],74)!=static_cast<unsigned>(requested+150)%75 || (adr==3 && (control&4)))
            throw std::runtime_error("Invalid non-positional gap Q record");
    } else throw std::runtime_error("Unsupported Q mode in gap");
    return (control&4)?2u:1u;
}
Bytes decode_raw_q(const Bytes& subchannels) {
    if (subchannels.size() != 96) throw std::runtime_error("Short raw subchannel response");
    Bytes q(16, 0);
    for (unsigned bit = 0; bit < 96; ++bit)
        q[bit / 8] |= static_cast<std::uint8_t>(((subchannels[bit] >> 6) & 1) << (7 - bit % 8));
    validate_q(q);
    return q;
}
Position parse_q(const Bytes& bytes) {
    validate_q(bytes);
    if ((bytes[0] & 15) != 1 || bytes[6] != 0)
        throw std::runtime_error("Q does not contain a positional record");
    Position p;
    p.control = bytes[0] >> 4;
    p.track = bcd(bytes[1], 99);
    p.index = bcd(bytes[2], 99);
    if (p.track == 0 || p.index > 1) throw std::runtime_error("Unsupported Q track/index");
    p.relative = msf(bytes.data() + 3);
    p.absolute = msf(bytes.data() + 7) - 150;
    return p;
}
std::array<std::uint8_t, 12> read_cd_command(std::int32_t lba, unsigned sectors,
                                          unsigned subchannel,unsigned expected_type) {
    if (lba < -150 || lba > 449849 || sectors == 0 || sectors > 32 ||
        static_cast<std::int64_t>(lba) + sectors > 449850 ||
        (subchannel != 0 && subchannel != 1 && subchannel != 2) || expected_type<1 || expected_type>2)
        throw std::runtime_error("Invalid READ CD request");
    std::array<std::uint8_t, 12> cdb{};
    cdb[0] = 0xbe;
    cdb[1] = static_cast<std::uint8_t>(expected_type<<2);
    const auto address = static_cast<std::uint32_t>(lba);
    for (unsigned n = 0; n < 4; ++n) cdb[2 + n] = static_cast<std::uint8_t>(address >> (24 - 8 * n));
    cdb[8] = static_cast<std::uint8_t>(sectors);
    cdb[9] = 0xf8; // Sync, all headers, user data and EDC/ECC; ignored header flags for CD-DA.
    cdb[10] = static_cast<std::uint8_t>(subchannel);
    return cdb;
}
std::string hex(const Bytes& bytes) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (auto value : bytes) out << std::setw(2) << unsigned(value);
    return out.str();
}
}
