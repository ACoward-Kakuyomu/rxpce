#include "disc_image.h"
#include <cstdio>

int wmain(int argc, wchar_t** argv) {
    const bool allow = argc > 1 && std::wstring(argv[1]) == L"--allow-network";
    if (argc != (allow ? 4 : 3)) {
        std::fprintf(stderr,
                     "Usage: cue_materialize [--allow-network] <source.cue> <new-output-folder>\n");
        return 2;
    }
    const int first = allow ? 2 : 1;
    rxpce_cd::DiscImage image;
    std::string error;
    if (!image.open_cue(argv[first], error, {},
                        allow ? rxpce_cd::MediaAccess::allow_network
                              : rxpce_cd::MediaAccess::local_only) ||
        !image.write_materialized_cue(argv[first + 1], error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    std::wprintf(
        L"Created %ls/disc.cue and track BIN files. Convert this CUE with chdman createcd.\n",
        argv[first + 1]);
}
