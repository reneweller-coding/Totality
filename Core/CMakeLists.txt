add_library(TotalityCore STATIC
    src/Params.cpp
    src/Clock.cpp
    src/Score.cpp
    src/Midi.cpp
    src/WavWriter.cpp
    src/Halfband.cpp
    src/Loudness.cpp
    src/Leveler.cpp
    src/fx/Dynamics.cpp
    src/fx/Reverb.cpp
    src/synth/Kick.cpp
    src/synth/Rumble.cpp
    src/synth/SubBass.cpp
    src/synth/Kit.cpp
    src/synth/Ping.cpp
    src/synth/Synth.cpp
    src/synth/Chord.cpp
    src/synth/Drone.cpp
    src/fx/TapeEcho.cpp
    src/fx/Spring.cpp
    src/fx/Plate.cpp
    src/fx/Dub.cpp
    src/fx/Cloud.cpp
    src/pattern/Rack.cpp
    src/compose/Study.cpp
    src/compose/Style.cpp
    src/compose/GestureEngine.cpp
    src/compose/Corridor.cpp
    src/compose/Composer.cpp
    src/compose/Set.cpp
    src/SetFile.cpp
    src/Deck.cpp
    src/Engine.cpp
    src/Cue.cpp
    src/Export.cpp
    src/Presets.cpp
    src/Preferences.cpp
)
target_include_directories(TotalityCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
option(TOT_PROFILE "Time the engine's stages (Profile.h); tot_render prints them" OFF)
if(TOT_PROFILE)
    target_compile_definitions(TotalityCore PUBLIC TOT_PROFILE)
endif()
if(WIN32)
    target_link_libraries(TotalityCore PUBLIC ws2_32)   # the cue sender's UDP socket (Cue.h)
endif()
target_compile_features(TotalityCore PUBLIC cxx_std_20)
# Position-independent (02.10.2026): on Linux the VST3 is a shared object the core is linked into, and
# GCC does not make a static library position-independent by itself (Windows and macOS need nothing).
set_target_properties(TotalityCore PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(TotalityCore PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(TotalityCore PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(TotalityCore PRIVATE -Wall -Wextra)
endif()

# AVX2 with FMA for the lane-parallel paths (Vec.h). PUBLIC, so everything that links the core sees the same vector
# path the core was built with. The arm64 build (Quest) takes NEON on its own.
if(TOT_AVX2 AND CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|AMD64|x64)")
    if(MSVC)
        target_compile_options(TotalityCore PUBLIC /arch:AVX2)
    else()
        target_compile_options(TotalityCore PUBLIC -mavx2 -mfma)
    endif()
endif()
