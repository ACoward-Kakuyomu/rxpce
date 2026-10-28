#define NOMINMAX
#include "transport.h"
#include "layout.h"
#include "store.h"
#include <windows.h>
#include <picojson.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <exception>
#include <set>

namespace dump = rxpce_cd_dump;
namespace {
std::atomic<bool> stopRequested{false};
BOOL WINAPI control_handler(DWORD event) {
    if (event==CTRL_C_EVENT || event==CTRL_BREAK_EVENT) {
        stopRequested.store(true,std::memory_order_relaxed);return TRUE;
    }
    return FALSE;
}
void check_cancel() {
    if (stopRequested.load(std::memory_order_relaxed)) throw dump::Cancelled();
}
picojson::value number(std::int64_t n) { return picojson::value(static_cast<double>(n)); }
picojson::value string(const std::string& s) { return picojson::value(s); }
picojson::value probe(dump::Transport& drive) {
    const auto profile = dump::read_profile(drive);
    dump::read_session(drive);
    const auto toc = dump::read_toc(drive);
    picojson::object root;
    root["tool"] = string("rxpce_cd_dump");
    root["stage"] = string("probe");
    root["media_profile"] = number(profile);
    root["leadout_lba"] = number(toc.leadout);
    picojson::array tracks;
    std::set<std::int32_t> points{-150, -1, 0};
    for (const auto& track : toc.tracks) {
        picojson::object t;
        t["number"] = number(track.number); t["control"] = number(track.control);
        t["index1_lba"] = number(track.index1);
        tracks.emplace_back(t);
        for (auto lba = track.index1 - 1; lba <= track.index1 + 2 && lba < toc.leadout; ++lba)
            points.insert(lba);
    }
    root["tracks"] = picojson::value(tracks);
    picojson::array samples;
    for (auto lba : points) {
        check_cancel();
        picojson::object sample; sample["requested_lba"] = number(lba);
        try {
            const auto bytes = dump::read_q_frame(drive,lba);
            const dump::Bytes rawQ(bytes.begin() + 2352, bytes.end());
            dump::validate_q(rawQ);
            sample["q_adr"] = number(rawQ[0] & 15);
            if ((rawQ[0] & 15) == 1) {
                const auto q = dump::parse_q(rawQ);
                sample["q_lba"] = number(q.absolute); sample["q_relative"] = number(q.relative);
                sample["q_index"] = number(q.index); sample["q_track"] = number(q.track);
                sample["q_control"] = number(q.control);
            }
            sample["q_crc_valid"] = picojson::value(true);
            sample["raw_sync"] = picojson::value(bytes[0] == 0 && bytes[11] == 0 &&
                std::all_of(bytes.begin() + 1, bytes.begin() + 11, [](auto b) { return b == 255; }));
        } catch (const dump::Cancelled&) { throw; }
        catch (const std::exception& e) { sample["error"] = string(e.what()); }
        samples.emplace_back(sample);
    }
    root["samples"] = picojson::value(samples);
    root["positional_q_read_mode"] = string("CD-DA main plus Q; main bytes discarded");
    picojson::array rawSamples;
    for (const auto& track : toc.tracks) {
        check_cancel();
        if (!rawSamples.empty() && !track.data()) continue;
        picojson::object sample;
        sample["requested_lba"] = number(track.index1 + 8);
        try {
            const auto bytes = dump::read_cd(drive,track.index1+8,1,1,track.data()?2:1);
            const auto q = dump::decode_raw_q(dump::Bytes(bytes.begin() + 2352, bytes.end()));
            sample["q_crc_valid"] = picojson::value(true);
            sample["q_adr"] = number(q[0] & 15);
            if (track.data()) {
                dump::validate_mode1(bytes.data(),track.index1+8);
                sample["mode1_header_edc_ecc_valid"] = picojson::value(true);
            }
        } catch (const dump::Cancelled&) { throw; }
        catch (const std::exception& e) { sample["error"] = string(e.what()); }
        rawSamples.emplace_back(sample);
    }
    root["raw_subchannel_samples"] = picojson::value(rawSamples);
    return picojson::value(root);
}
}
int wmain(int argc, wchar_t** argv) {
    try {
        if(!SetConsoleCtrlHandler(control_handler,TRUE))
            throw std::runtime_error("Cannot register the console cancellation handler");
        if (argc == 2 && std::wstring(argv[1]) == L"list") {
            picojson::array result;
            for (const auto& drive : dump::list_drives()) {
                picojson::object item;
                item["path"] = string(dump::utf8(drive.path)); item["name"] = string(drive.name);
                result.emplace_back(item);
            }
            std::puts(picojson::value(result).serialize(true).c_str());
            return 0;
        }
        if ((argc == 4 || (argc == 5 && std::wstring(argv[4]) == L"--layout")) &&
            std::wstring(argv[1]) == L"probe" && std::wstring(argv[2]) == L"--drive") {
            dump::WindowsDrive drive(argv[3]);
            const auto identity = drive.inquiry();
            std::fputs("Probing optical media...\n",stderr);
            auto report = probe(drive);
            report.get<picojson::object>()["device"] = string(identity);
            report.get<picojson::object>()["device_path"] = string(dump::utf8(argv[3]));
            const auto generation=drive.media_generation();
            report.get<picojson::object>()["media_generation"] = generation ? number(*generation) : picojson::value();
            if (argc == 5) {
                std::fputs("Scanning track/index boundaries...\n", stderr);
                dump::Layout layout;
                try { layout = dump::discover_layout(drive,check_cancel); }
                catch (const dump::Cancelled&) { throw; }
                catch (const std::exception& e) {
                    report.get<picojson::object>()["layout_error"] = string(e.what());
                    std::puts(report.serialize(true).c_str());
                    std::fprintf(stderr, "%s\n", e.what());
                    return 1;
                }
                picojson::array ranges;
                for (const auto& track : layout.tracks) {
                    picojson::object range;
                    range["number"] = number(track.toc.number);
                    range["begin_lba"] = number(track.begin);
                    range["index1_lba"] = number(track.toc.index1);
                    range["end_lba"] = number(track.end);
                    ranges.emplace_back(range);
                }
                auto& object = report.get<picojson::object>();
                object["capture_layout"] = picojson::value(ranges);
                object["observations"] = number(static_cast<std::int64_t>(layout.evidence.size()));
                object["nonposition_records"] = number(layout.nonposition_records);
                object["q_retries"] = number(layout.q_retries);
                object["candidate_cue"] = string(dump::make_cue(layout));
            }
            std::puts(report.serialize(true).c_str());
            return 0;
        }
        if (argc==6 && std::wstring(argv[1])==L"read" && std::wstring(argv[2])==L"--drive" &&
            std::wstring(argv[4])==L"--output") {
            dump::WindowsDrive drive(argv[3]);
            const auto identity=drive.inquiry();
            const auto locked=drive.lock_media();
            std::fprintf(stderr,"Removal lock: %s\nScanning track/index boundaries...\n",locked?"active":"unavailable");
            const auto layout=dump::discover_layout(drive,check_cancel);
            dump::FileStore output(layout,argv[5],identity+" @ "+dump::utf8(argv[3]),check_cancel);
            auto last=std::chrono::steady_clock::now();
            bool previousPass=true;unsigned previousTrack=0;
            auto previousPhase=dump::CapturePhase::finalizing;
            const auto progress=[&](const dump::CaptureProgress& p) {
                const auto now=std::chrono::steady_clock::now();
                if (p.phase!=dump::CapturePhase::retrying && p.phase==previousPhase &&
                    p.verifying==previousPass && p.track==previousTrack && p.completed!=p.total &&
                    now-last<std::chrono::milliseconds(500)) return;
                last=now;previousPass=p.verifying;previousTrack=p.track;previousPhase=p.phase;
                const auto* label=p.phase==dump::CapturePhase::finalizing?"Finalizing":
                    p.phase==dump::CapturePhase::retrying?(p.attempt?"Retrying":"Splitting batch"):
                    p.verifying?"Verifying":"Reading";
                std::fprintf(stderr,"\r%s",label);
                if(p.track) std::fprintf(stderr," track %02u",p.track);
                std::fprintf(stderr,": %llu/%llu sectors (%.1f%%)",
                    static_cast<unsigned long long>(p.completed),static_cast<unsigned long long>(p.total),
                    p.total?100.0*static_cast<double>(p.completed)/static_cast<double>(p.total):0.0);
                if(p.phase==dump::CapturePhase::retrying)
                    std::fprintf(stderr," LBA %d",p.lba);
                if(p.attempt) std::fprintf(stderr," attempt %u",p.attempt);
                if (p.completed==p.total) std::fputc('\n',stderr);
            };
            dump::capture(drive,layout,output,{},progress,check_cancel);
            std::printf("Created %s/disc.cue; repeated RAW reads match.\n",dump::utf8(argv[5]).c_str());
            return 0;
        }
        std::fputs("Usage: rxpce_cd_dump list\n       rxpce_cd_dump probe --drive E: [--layout]\n"
                   "       rxpce_cd_dump read --drive E: --output <new-folder>\n", stderr);
        return 2;
    } catch (const dump::Cancelled& e) { std::fprintf(stderr,"\n%s\n",e.what());return 3; }
    catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
