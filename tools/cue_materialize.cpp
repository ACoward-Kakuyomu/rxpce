#include "disc_image.h"
#include <cstdio>

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "Usage: cue_materialize <source.cue> <new-output-folder>\n");
        return 2;
    }
    rxpce_cd::DiscImage image;
    std::string error;
    if (!image.open_cue(argv[1], error) || !image.write_materialized_cue(argv[2], error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    std::wprintf(
        L"Created %ls/disc.cue and track BIN files. Convert this CUE with chdman createcd.\n",
        argv[2]);
}
