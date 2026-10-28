cmake_minimum_required(VERSION 3.20)
project(Starter3DEngine VERSION 0.2.0 LANGUAGES CXX)

# Ninja/MinGW is a single-config generator; make a plain configure optimized by default.
# Otherwise `cmake --build ... --config Release` does not enable optimization there.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(engine_core
    src/engine/scene.cpp
    src/engine/software_renderer.cpp
)
target_include_directories(engine_core PUBLIC include)
target_compile_features(engine_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(engine_core PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(engine_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    add_executable(engine_editor WIN32 src/editor/main.cpp)
    target_link_libraries(engine_editor PRIVATE engine_core user32 gdi32)
    target_compile_definitions(engine_editor PRIVATE UNICODE _UNICODE)
    if(MINGW)
        # MinGW needs this option to use the Unicode wWinMain entry point.
        target_link_options(engine_editor PRIVATE -municode)
    endif()
    if(MSVC)
        target_compile_options(engine_editor PRIVATE /W4 /permissive- /utf-8)
    endif()
else()
    # Headless Linux build uses the same project-owned renderer and scene core.
    add_executable(engine_editor src/editor/linux_main.cpp)
    target_link_libraries(engine_editor PRIVATE engine_core)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(engine_tests tests/engine_tests.cpp)
    target_link_libraries(engine_tests PRIVATE engine_core)
    add_test(NAME engine_tests COMMAND engine_tests)
endif()
