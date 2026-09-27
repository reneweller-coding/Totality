# Umbra -- a generator for hypnotic Berlin techno, tracks and DJ sets (docs/PLAN.md).
#
# Layout:
#   Core/         framework-free engine (static library UmbraCore)
#   Tools/render  umb_render: offline render, MIDI export, stems
#   Tests/        umb_selftest and the vector-path variants of umb_vectest
#   Plugin/       JUCE VST3 + Standalone (Phase 5)
#   Quest/        native OpenXR app (Phase 6)
#
# Built after Ephemeris's root CMakeLists.txt at d047d79 (27.09.2026); the rules below are its rules.
cmake_minimum_required(VERSION 3.22)
project(Umbra VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# One source of truth for the version: this project() line.
add_compile_definitions(UMB_VERSION="${PROJECT_VERSION}")

option(UMB_BUILD_TOOLS "Build umb_render and the tests" ON)
option(UMB_AVX2 "Build the x86-64 core with AVX2 + FMA" ON)
option(UMB_STATIC_RUNTIME "Link the MSVC runtime statically (for distribution)" OFF)

if(MSVC)
    add_compile_options(/MP)
endif()
if(UMB_STATIC_RUNTIME AND MSVC)
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
    set(UMB_AVX2 OFF CACHE BOOL "" FORCE)
endif()

add_subdirectory(Core)
if(UMB_BUILD_TOOLS)
    enable_testing()
    add_subdirectory(Tools/render)
    add_subdirectory(Tests)
endif()
