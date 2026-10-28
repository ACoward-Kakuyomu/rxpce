# Keep upstream sources unchanged. Options apply only to this dependency scope.
function(rxpce_add_chd_dependencies)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(BUILD_SHARED_LIBS OFF)
    set(INSTALL_STATIC_LIBS OFF)
    set(WITH_SYSTEM_ZLIB OFF)
    set(WITH_SYSTEM_ZSTD OFF)
    set(WITH_LZMA_ASM OFF)
    set(BUILD_LTO OFF)
    set(BUILD_FUZZER OFF)
    set(CHDR_WANT_RAW_DATA_SECTOR ON)
    set(CHDR_WANT_SUBCODE ON)
    set(CHDR_VERIFY_BLOCK_CRC ON)
    set(MINIZ_ARCHIVE_APIS ON)
    set(MINIZ_DEFLATE_APIS ON)
    set(MINIZ_STDIO OFF)
    set(MINIZ_TIME OFF)
    add_subdirectory(third-party/libchdr EXCLUDE_FROM_ALL)
    # Upstream miniz does not export its include directory; all consumers must
    # share both its header and PUBLIC feature definitions (notably NO_TIME).
    target_include_directories(miniz SYSTEM PUBLIC
        "${PROJECT_SOURCE_DIR}/third-party/libchdr/deps/miniz-3.1.1")
endfunction()
rxpce_add_chd_dependencies()
