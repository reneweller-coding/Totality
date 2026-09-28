# Totality for Meta Quest -- native OpenXR app (NativeActivity, GLES 3, Oboe), no game engine.
# Configured by build_apk.ps1 with the NDK toolchain; see Quest/README.md. After Ephemeris' Quest/CMakeLists.txt.
cmake_minimum_required(VERSION 3.22)
project(TotalityQuest LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(TOT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(TOT_THIRDPARTY "${TOT_ROOT}/ThirdParty" CACHE PATH "OpenXR loader and Oboe (Quest/fetch_thirdparty.ps1)")

# The same rule as the root CMakeLists: no floating-point contraction. Without it clang fuses a*b+c into an
# FMA, and the scalar reference stops matching the NEON lanes (Vec.h, Tests/vectest.cpp).
add_compile_options(-ffp-contract=off)

# The engine, unchanged; TOT_AVX2 means nothing on arm64 (Core/CMakeLists.txt guards it on the processor).
set(TOT_AVX2 OFF CACHE BOOL "" FORCE)
add_subdirectory("${TOT_ROOT}/Core" Core)

# Khronos OpenXR loader for Android: the prefab layout of the Maven AAR (headers + libopenxr_loader.so).
set(OXR "${TOT_THIRDPARTY}/openxr-loader/prefab/modules")
if(NOT EXISTS "${OXR}/openxr_loader/libs/android.${ANDROID_ABI}/libopenxr_loader.so")
    message(FATAL_ERROR "OpenXR loader missing under ${TOT_THIRDPARTY}: run Quest/fetch_thirdparty.ps1")
endif()
add_library(openxr_loader SHARED IMPORTED)
set_target_properties(openxr_loader PROPERTIES
    IMPORTED_LOCATION "${OXR}/openxr_loader/libs/android.${ANDROID_ABI}/libopenxr_loader.so"
    INTERFACE_INCLUDE_DIRECTORIES "${OXR}/headers/include")

# Oboe (low-latency Android audio), built from source as a static library.
add_subdirectory("${TOT_THIRDPARTY}/oboe" oboe)

# The NDK's native activity glue.
add_library(native_app_glue STATIC "${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c")
target_include_directories(native_app_glue PUBLIC "${ANDROID_NDK}/sources/android/native_app_glue")

add_library(totquest SHARED src/main.cpp)
target_compile_definitions(totquest PRIVATE XR_USE_PLATFORM_ANDROID XR_USE_GRAPHICS_API_OPENGL_ES)
target_link_libraries(totquest PRIVATE TotalityCore openxr_loader oboe native_app_glue android log EGL GLESv3)
# ANativeActivity_onCreate lives in the glue library and nothing in our code references it.
target_link_options(totquest PRIVATE -u ANativeActivity_onCreate)
target_compile_options(totquest PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
