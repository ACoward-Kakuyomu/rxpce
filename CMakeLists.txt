cmake_minimum_required(VERSION 3.25)
project(rxpce VERSION 1.0.0 LANGUAGES C CXX RC)

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8
   OR NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
    message(FATAL_ERROR "This build requires Windows x64 and MSVC with ATL installed.")
endif()

include(cmake/chd_dependencies.cmake)

# Apply the language baseline to our targets, after creating upstream targets.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(mcs6502 STATIC third-party/MCS6502/MCS6502.c)
target_compile_features(mcs6502 PRIVATE c_std_11)
target_include_directories(mcs6502 SYSTEM PUBLIC third-party/MCS6502)

add_library(huc6280 STATIC src/huc6280.cpp)
target_compile_features(huc6280 PUBLIC cxx_std_17)
target_include_directories(huc6280 PUBLIC src)
target_link_libraries(huc6280 PUBLIC mcs6502)
add_library(pce_cpu STATIC src/pce_cpu.cpp src/pce_cpu_state.cpp)
target_link_libraries(pce_cpu PUBLIC huc6280)
add_library(machine_state STATIC src/io_state_codec.cpp src/machine_state_codec.cpp)
target_include_directories(machine_state PUBLIC src)
target_link_libraries(machine_state PUBLIC pce_cpu)
add_library(cartridge STATIC src/cartridge.cpp src/cartridge_ram.cpp src/system_card.cpp src/nvram.cpp src/disc_image.cpp src/disc_cue.cpp src/disc_chd.cpp src/cd_controller.cpp src/cd_controller_state.cpp src/cd_state_file.cpp src/cd_session.cpp)
target_compile_features(cartridge PUBLIC cxx_std_17)
target_include_directories(cartridge PUBLIC src)
target_compile_definitions(cartridge PRIVATE _WIN32_WINNT=0x0A00)
target_link_libraries(cartridge PRIVATE bcrypt chdr-static)
add_library(machine_core STATIC src/machine.cpp src/vdc_render.cpp src/machine_devices.cpp)
target_link_libraries(machine_core PUBLIC cartridge pce_cpu)
target_compile_options(machine_core PRIVATE /utf-8)
add_library(host_paths STATIC src/file_paths.cpp)
target_include_directories(host_paths PUBLIC src)

add_executable(cue_materialize tools/cue_materialize.cpp)
target_compile_features(cue_materialize PRIVATE cxx_std_17)
target_link_libraries(cue_materialize PRIVATE cartridge)

add_executable(rxpce WIN32
    src/settings.cpp src/settings_dialog.cpp src/AboutDlg.cpp src/archive.cpp src/video.cpp src/debug.cpp src/frame_timing.cpp
    src/di.cpp src/ds.cpp src/wasapi.cpp src/video_info.cpp src/log.cpp src/d3d11_renderer.cpp
    src/MainFrame.cpp src/Pce.cpp src/Sound.cpp src/cd_prepare_dialog.cpp src/state_service.cpp src/machine_runtime.cpp
    src/host_input.cpp src/host_commands.cpp src/host_runtime.cpp src/rxpce.cpp src/rxpce.rc)
target_compile_features(rxpce PRIVATE cxx_std_17)
target_compile_definitions(rxpce PRIVATE
    _MBCS _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
    USE_JOYSTICK SOUND DIRECTINPUT_VERSION=0x0800)
target_include_directories(rxpce PRIVATE src)
target_include_directories(rxpce SYSTEM PRIVATE third-party/wtl/Include third-party/picojson)
target_link_libraries(rxpce PRIVATE host_paths machine_core machine_state cartridge miniz dxguid d3d11 dxgi d3dcompiler winmm dinput8 xinput dsound avrt comctl32 comdlg32 ole32 shell32 uuid)
target_link_options(rxpce PRIVATE "/MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/src/rxpce.manifest")
function(rxpce_copy_notices target)
foreach(document IN ITEMS
    LICENSE.md README.md MANUAL.md BUILDING.md CUE_MATERIALIZE.md CD_DUMP.md THIRD_PARTY_NOTICES.md
    LICENSES/fpce-xpce.txt LICENSES/PolyForm-Noncommercial-1.0.0.txt
    third-party/MCS6502/LICENSE.TXT
    third-party/libchdr/LICENSES.rxpce.txt third-party/libchdr/README.rxpce.md
    third-party/wtl/MS-PL.txt third-party/picojson/LICENSE)
get_filename_component(document_directory "${document}" DIRECTORY)
add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/${document_directory}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/${document}" "$<TARGET_FILE_DIR:${target}>/${document}"
    VERBATIM)
endforeach()
endfunction()
rxpce_copy_notices(rxpce)
rxpce_copy_notices(cue_materialize)

option(RXPCE_BUILD_CD_DUMP "Build standalone physical CD capture tool" ON)
if(RXPCE_BUILD_CD_DUMP)
    add_library(cd_dump_model STATIC src/cd_dump/model.cpp src/cd_dump/reader.cpp src/cd_dump/layout.cpp src/cd_dump/capture.cpp)
    target_include_directories(cd_dump_model PUBLIC src)
    add_library(cd_dump_store STATIC src/cd_dump/store.cpp)
    target_link_libraries(cd_dump_store PUBLIC cd_dump_model PRIVATE bcrypt)
    target_include_directories(cd_dump_store SYSTEM PRIVATE third-party/picojson)
    add_executable(rxpce_cd_dump src/cd_dump/main.cpp src/cd_dump/windows_drive.cpp)
    target_link_libraries(rxpce_cd_dump PRIVATE cd_dump_store setupapi uuid)
    target_compile_definitions(rxpce_cd_dump PRIVATE WINVER=0x0A00 _WIN32_WINNT=0x0A00)
    target_include_directories(rxpce_cd_dump SYSTEM PRIVATE third-party/picojson)
    rxpce_copy_notices(rxpce_cd_dump)
endif()


# Only this directory's C++ targets; upstream C/C++ targets keep their policy.
get_property(rxpce_own_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(target IN LISTS rxpce_own_targets)
    get_target_property(kind ${target} TYPE)
    if(NOT kind STREQUAL "UTILITY")
        target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/W4;/external:W0;/utf-8>")
    endif()
endforeach()
