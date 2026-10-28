# Vendored libchdr

* Upstream: https://github.com/rtissera/libchdr
* Version: v0.3.0
* Commit: `93d8c239ff0d4e8d7722985992649fce12d2463b`
* Archive: https://codeload.github.com/rtissera/libchdr/zip/93d8c239ff0d4e8d7722985992649fce12d2463b
* Archive SHA-256: `59238fbd59428d2b540f41149de5dc0794d8d9050ebc1ae82d24096f3f60ab62`

All upstream files are unchanged; SHA256SUMS.upstream lists their byte hashes.
Local additions are this file, .gitattributes, SHA256SUMS.upstream,
LICENSES.rxpce.txt, and deps/zstd-1.5.7/LICENSE. The latter comes from
https://raw.githubusercontent.com/facebook/zstd/v1.5.7/LICENSE
(SHA-256 `7055266497633c9025b777c78eb7235af13922117480ed5c674677adc381c9d8`).
The notice collection reproduces the licenses from the retained sources and
the Zstandard license. We select Zstandard's BSD license and dr_flac's MIT No
Attribution alternative. Upstream copyright/license comments remain intact.

Integration is in ../../cmake/chd_dependencies.cmake. Static libraries only;
no network access or system packages are required at configure/build time.
ZIP and CHD share the bundled miniz 3.1.1 target with archive/deflate enabled,
stdio/time disabled, and its PUBLIC feature definitions propagated to users.
LZMA SDK 25.01 uses its C implementation without assembly. Zstandard 1.5.7
and dr_flac 0.13.3 are retained. Raw CD data, subcode, and block CRC checks
are enabled. Upstream benchmark/fuzzer executables are not in the default
build; the project owns its CTest tests.

This dependency import does not add application-level CHD media support.
compression_dependencies_tests checks an original synthetic uncompressed v5
image, an independently compressed v4 fixture, and the application's ZIP
reader in one executable. The v4 fixture only tests the library's DEFLATE
integration; it does not expand the planned application's v5-only scope.
