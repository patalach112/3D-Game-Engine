cmake_minimum_required(VERSION 3.20)
project(Starter3DEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "Do not build raylib examples" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "Do not build raylib games" FORCE)
FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

add_library(engine_core src/engine/scene.cpp)
target_include_directories(engine_core PUBLIC include)
target_compile_features(engine_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(engine_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(engine_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(engine_editor src/editor/main.cpp)
target_link_libraries(engine_editor PRIVATE engine_core raylib)
if(MSVC)
    target_compile_options(engine_editor PRIVATE /utf-8)
endif()
add_custom_command(TARGET engine_editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:engine_editor>/assets"
    COMMENT "Copying editor assets beside the executable"
)

include(CTest)
if(BUILD_TESTING)
    add_executable(engine_tests tests/engine_tests.cpp)
    target_link_libraries(engine_tests PRIVATE engine_core)
    add_test(NAME engine_tests COMMAND engine_tests)
endif()
