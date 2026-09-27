add_library(UmbraCore STATIC
    src/Params.cpp
    src/Clock.cpp
    src/Score.cpp
    src/Midi.cpp
    src/WavWriter.cpp
    src/Halfband.cpp
    src/Loudness.cpp
    src/fx/Dynamics.cpp
    src/fx/Reverb.cpp
    src/synth/Kick.cpp
    src/synth/Rumble.cpp
    src/synth/SubBass.cpp
    src/synth/Kit.cpp
    src/pattern/Rack.cpp
    src/compose/Study.cpp
    src/Engine.cpp
)
target_include_directories(UmbraCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(UmbraCore PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(UmbraCore PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(UmbraCore PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(UmbraCore PRIVATE -Wall -Wextra)
endif()

# AVX2 with FMA for the lane-parallel paths (Vec.h). PUBLIC, so everything that links the core sees the same vector
# path the core was built with. The arm64 build (Quest) takes NEON on its own.
if(UMB_AVX2 AND CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|AMD64|x64)")
    if(MSVC)
        target_compile_options(UmbraCore PUBLIC /arch:AVX2)
    else()
        target_compile_options(UmbraCore PUBLIC -mavx2 -mfma)
    endif()
endif()
