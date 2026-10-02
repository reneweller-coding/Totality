# Totality -- a generator for hypnotic Berlin techno, tracks and DJ sets (docs/PLAN.md).
#
# Layout:
#   Core/         framework-free engine (static library TotalityCore)
#   Tools/render  tot_render: offline render, MIDI export, stems
#   Tests/        tot_selftest and the vector-path variants of tot_vectest
#   Plugin/       JUCE VST3 + Standalone (Phase 5)
#   Quest/        native OpenXR app (Phase 6)
#
# Built after Ephemeris's root CMakeLists.txt at d047d79 (27.09.2026); the rules below are its rules.
cmake_minimum_required(VERSION 3.22)
project(Totality VERSION 1.1.0 LANGUAGES C CXX)

# The family's layout (01.10.2026): build/<preset>, bin/<preset>, dist/, work/ -- cmake/Family.cmake,
# CMakePresets.json and build.ps1, the same in every repository of the family.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Family.cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# One source of truth for the version: this project() line.
add_compile_definitions(TOT_VERSION="${PROJECT_VERSION}")

option(TOT_BUILD_TOOLS "Build tot_render and the tests" ON)
# The plugin is a desktop artefact (JUCE cannot cross-compile its helper for Android).
if(ANDROID)
    option(TOT_BUILD_PLUGIN "Build the JUCE plugin (VST3 + Standalone)" OFF)
else()
    option(TOT_BUILD_PLUGIN "Build the JUCE plugin (VST3 + Standalone)" ON)
endif()
set(TOT_JUCE_TAG "9.0.1" CACHE STRING "JUCE git tag to fetch when no local checkout is found")
option(TOT_AVX2 "Build the x86-64 core with AVX2 + FMA" ON)
option(TOT_STATIC_RUNTIME "Link the MSVC runtime statically (for distribution)" OFF)

if(MSVC)
    add_compile_options(/MP)
endif()
if(TOT_STATIC_RUNTIME AND MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# No fast-math and no floating-point contraction: the offline render is the determinism oracle and the lane paths
# (Vec.h) are tested bit for bit against the scalar path. Clang contracts a*b+c by default; Intel's icx does even
# under /fp:precise (measured in Phosphene, 23.09.2026).
if(NOT MSVC)
    add_compile_options(-ffp-contract=off)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
    add_compile_options(/fp:precise /Qfma- /clang:-Wno-overriding-option)
endif()
if(ANDROID)
    set(TOT_AVX2 OFF CACHE BOOL "" FORCE)
endif()

add_subdirectory(Core)
if(TOT_BUILD_TOOLS)
    enable_testing()
    family_doccheck()   # every entity documented (cmake/Family.cmake, 01.10.2026)
    add_subdirectory(Tools/render)
    family_soundcheck(tot_render)   # the sound regression check (Tools/soundcheck.py, 02.10.2026)
    add_subdirectory(Tests)
endif()

# The plugin (Phase 5). JUCE from ThirdParty/JUCE, else from the sibling Phosphene's checkout, else fetched. Nothing
# under Core/ includes JUCE.
if(TOT_BUILD_PLUGIN)
    family_juce(${TOT_JUCE_TAG})   # ThirdParty/JUCE, the sibling Phosphene's, or fetched (cmake/Family.cmake)
    add_subdirectory(Plugin)
    # The built VST3 loaded the way a DAW loads it (Tests/vst3test.cpp). Here and not in Tests/, because the plugin
    # target has to exist first.
    if(TOT_BUILD_TOOLS)
        add_executable(tot_vst3test Tests/vst3test.cpp)
        target_link_libraries(tot_vst3test PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
        target_compile_definitions(tot_vst3test PRIVATE JUCE_STANDALONE_APPLICATION=1 JUCE_PLUGINHOST_VST3=1
                                   JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
                                   JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)
        add_dependencies(tot_vst3test Totality_VST3)
        if(MSVC)
            target_compile_options(tot_vst3test PRIVATE /W4 /permissive- /utf-8)
            target_link_options(tot_vst3test PRIVATE "LINKER:/STACK:16777216")
        endif()
        add_test(NAME vst3test COMMAND tot_vst3test "$<TARGET_FILE:Totality_VST3>")
        set_tests_properties(vst3test PROPERTIES LABELS "audio;slow" PROCESSORS 3 ENVIRONMENT_MODIFICATION "TOT_MUTE=unset:")
        # Tracktion's pluginval (v1.0.4, unpacked into ThirdParty/pluginval or the sibling Ephemeris's, not in the
        # repository) at its highest strictness, where it is there. Muted: it plays the plugin through its own tests only.
        set(TOT_PLUGINVAL "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/pluginval/pluginval.exe")
        if(NOT EXISTS "${TOT_PLUGINVAL}")
            set(TOT_PLUGINVAL "${CMAKE_CURRENT_SOURCE_DIR}/../BerlinSchoolGenerator/ThirdParty/pluginval/pluginval.exe")
        endif()
        if(EXISTS "${TOT_PLUGINVAL}")
            add_test(NAME pluginval COMMAND "${TOT_PLUGINVAL}" --strictness-level 10 --timeout-ms 900000
                     --validate "$<TARGET_FILE_DIR:Totality_VST3>/../..")
            set_tests_properties(pluginval PROPERTIES LABELS "audio;slow" PROCESSORS 3 ENVIRONMENT "TOT_MUTE=1")
        endif()
    endif()
endif()
