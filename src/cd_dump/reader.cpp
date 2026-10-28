#include "transport.h"
#include <stdexcept>
namespace rxpce_cd_dump {
Toc read_toc(Transport& drive) {
    return parse_toc(drive.execute({0x43,0,0,0,0,0,0,3,0x24,0}, 804));
}
void read_session(Transport& drive) {
    validate_single_session(drive.execute({0x43,0,1,0,0,0,0,0,12,0}, 12));
}
unsigned read_profile(Transport& drive) {
    const auto bytes = drive.execute({0x46,0,0,0,0,0,0,0,8,0}, 8);
    if (bytes.size() != 8) throw std::runtime_error("Short GET CONFIGURATION header");
    return unsigned(bytes[6]) * 256 + bytes[7];
}
Bytes read_cd(Transport& drive, std::int32_t lba, unsigned count, unsigned subchannel,unsigned expected_type) {
    const auto cdb = read_cd_command(lba, count, subchannel,expected_type);
    const auto bytes = count * (2352 + (subchannel == 1 ? 96 : subchannel == 2 ? 16 : 0));
    auto result = drive.execute(Bytes(cdb.begin(), cdb.end()), bytes);
    if (result.size() != bytes) throw std::runtime_error("Short READ CD transfer");
    return result;
}
Bytes read_q_frame(Transport& drive,std::int32_t lba) {
    try { return read_cd(drive,lba,1,2,1); }
    catch(const ScsiFailure& e) {
        if(!e.mode_mismatch()) throw;
        return read_cd(drive,lba,1,2,2);
    }
}
}
