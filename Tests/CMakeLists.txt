# On Android the executables are cross-compiled for the headset; ctest must not run them here.
function(umb_add_test name)
    if(NOT ANDROID)
        add_test(NAME ${name} COMMAND ${ARGN})
    endif()
endfunction()

add_executable(umb_selftest selftest.cpp)
target_link_libraries(umb_selftest PRIVATE UmbraCore)
# One ctest test per self-test section, `selftest.<name>`, read from the registration table in selftest.cpp
# ({ "testName", testName }) when CMake configures (after Ephemeris).
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/selftest.cpp)
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/selftest.cpp UMB_SECTION_LINES REGEX "^    \{ \"test[A-Za-z0-9]+\", test[A-Za-z0-9]+ \},")
foreach(line ${UMB_SECTION_LINES})
    string(REGEX MATCH "\"(test[A-Za-z0-9]+)\"" _ "${line}")
    umb_add_test(selftest.${CMAKE_MATCH_1} umb_selftest --only ${CMAKE_MATCH_1})
endforeach()

# The lane paths against the scalar reference, bit for bit, once per vector path. The main variant is built as the
# core is (AVX2 on x86-64, NEON on the Quest); on x86 the NEON path also runs through neonshim/arm_neon.h, and the
# scalar path is forced. The variants compile what they test themselves: linking UmbraCore would bring its AVX2 flags.
add_executable(umb_vectest vectest.cpp)
target_link_libraries(umb_vectest PRIVATE UmbraCore)
umb_add_test(vectest umb_vectest)
set(UMB_TEST_TARGETS umb_selftest umb_vectest)

if(NOT ANDROID AND CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|AMD64|x64)")
    if(UMB_AVX2)
        target_compile_definitions(umb_vectest PRIVATE UMB_EXPECT_PATH="avx2")
    endif()
    foreach(variant neon scalar)
        add_executable(umb_vectest_${variant} vectest.cpp ${CMAKE_SOURCE_DIR}/Core/src/Halfband.cpp
                       ${CMAKE_SOURCE_DIR}/Core/src/synth/Kit.cpp ${CMAKE_SOURCE_DIR}/Core/src/Params.cpp)
        target_include_directories(umb_vectest_${variant} PRIVATE ${CMAKE_SOURCE_DIR}/Core/include)
        target_compile_features(umb_vectest_${variant} PRIVATE cxx_std_20)
        umb_add_test(vectest_${variant} umb_vectest_${variant})
        list(APPEND UMB_TEST_TARGETS umb_vectest_${variant})
    endforeach()
    target_include_directories(umb_vectest_neon BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/neonshim)
    target_compile_definitions(umb_vectest_neon PRIVATE UMB_NEON_SHIM=1 UMB_EXPECT_PATH="neon-shim")
    target_compile_definitions(umb_vectest_scalar PRIVATE UMB_FORCE_SCALAR=1 UMB_EXPECT_PATH="scalar")
elseif(ANDROID AND CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    target_compile_definitions(umb_vectest PRIVATE UMB_EXPECT_PATH="neon")
endif()

foreach(t ${UMB_TEST_TARGETS})
    if(MSVC)
        target_compile_options(${t} PRIVATE /W4 /permissive- /utf-8)
        target_compile_definitions(${t} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${t} PRIVATE -Wall -Wextra)
    endif()
endforeach()
