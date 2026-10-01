add_executable(tot_render main.cpp)
target_link_libraries(tot_render PRIVATE TotalityCore)
if(MSVC)
    target_compile_options(tot_render PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(tot_render PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Into bin/<preset> after every build (cmake/Family.cmake, 01.10.2026).
family_bin(tot_render)
