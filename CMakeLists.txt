cmake_minimum_required(VERSION 3.20)
project(Starter3DEngine VERSION 0.3.0 LANGUAGES CXX)

# Ninja/MinGW is a single-config generator; make a plain configure optimized by default.
# Otherwise `cmake --build ... --config Release` does not enable optimization there.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Vulkan SDK supplies headers/loader and glslc (or glslangValidator).
find_package(Vulkan 1.1 REQUIRED)
find_program(ENGINE_GLSLC NAMES glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
find_program(ENGINE_GLSLANG NAMES glslangValidator glslang HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
if(NOT ENGINE_GLSLC AND NOT ENGINE_GLSLANG)
    message(FATAL_ERROR "Install Vulkan SDK: glslc or glslangValidator is required to build the shaders.")
endif()
set(SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${SHADER_DIR}")
foreach(STAGE IN ITEMS vert frag)
    if(ENGINE_GLSLC)
        set(SHADER_COMMAND "${ENGINE_GLSLC}" --target-env=vulkan1.1)
    else()
        set(SHADER_COMMAND "${ENGINE_GLSLANG}" -V --target-env vulkan1.1)
    endif()
    add_custom_command(OUTPUT "${SHADER_DIR}/scene.${STAGE}.spv"
        COMMAND ${SHADER_COMMAND} "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/scene.${STAGE}"
            -o "${SHADER_DIR}/scene.${STAGE}.spv"
        DEPENDS "src/shaders/scene.${STAGE}"
        VERBATIM)
endforeach()
add_custom_command(OUTPUT "${SHADER_DIR}/scene_shaders.hpp"
    COMMAND "${CMAKE_COMMAND}" "-DINPUT_DIR=${SHADER_DIR}"
        "-DOUTPUT=${SHADER_DIR}/scene_shaders.hpp" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_spirv.cmake"
    DEPENDS "${SHADER_DIR}/scene.vert.spv" "${SHADER_DIR}/scene.frag.spv" cmake/embed_spirv.cmake
    VERBATIM)

add_library(engine_core
    src/engine/scene.cpp
    src/engine/software_renderer.cpp
    "${SHADER_DIR}/scene_shaders.hpp"
)
target_include_directories(engine_core PUBLIC include PRIVATE "${SHADER_DIR}")
target_link_libraries(engine_core PUBLIC Vulkan::Vulkan)
target_compile_features(engine_core PUBLIC cxx_std_20)

option(ENGINE_STATIC_MINGW_RUNTIME "Link MinGW runtimes into executables for redistribution" ON)
if(MINGW AND ENGINE_STATIC_MINGW_RUNTIME)
    # -static also selects libwinpthread.a if this GCC's libstdc++ needs it.
    # Vulkan stays an import library: vulkan-1.dll comes from the GPU driver.
    target_link_options(engine_core INTERFACE -static -static-libgcc -static-libstdc++)
endif()

if(MSVC)
    target_compile_options(engine_core PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(engine_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    add_executable(engine_editor WIN32
        src/editor/main.cpp
        src/editor/app.cpp
        src/editor/ui_gdi.cpp
        src/editor/ui.cpp
        src/editor/render.cpp
        src/editor/scene_io.cpp
        src/editor/interaction.cpp
        src/editor/gizmo.cpp
        src/editor/window.cpp
    )
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
    # Headless Linux build uses the same Vulkan renderer with GPU image readback.
    add_executable(engine_editor src/editor/linux_main.cpp)
    target_link_libraries(engine_editor PRIVATE engine_core)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(engine_tests tests/engine_tests.cpp)
    target_link_libraries(engine_tests PRIVATE engine_core)
    if(MSVC)
        target_compile_options(engine_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(engine_tests PRIVATE -UNDEBUG)
    endif()
    add_test(NAME engine_tests COMMAND engine_tests)
    if(MINGW AND ENGINE_STATIC_MINGW_RUNTIME AND CMAKE_OBJDUMP)
        # Unlike rendering tests, this check does not need a Vulkan driver/GPU.
        add_test(NAME engine_runtime_dependencies
            COMMAND "${CMAKE_COMMAND}" "-DEXECUTABLE=$<TARGET_FILE:engine_editor>"
                "-DOBJDUMP=${CMAKE_OBJDUMP}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_windows_runtime.cmake")
    endif()
endif()

# A small distributable ZIP: shaders are embedded, compiler runtimes are static
# by default with MinGW. Never package the developer's GPU driver/Vulkan SDK.
install(TARGETS engine_editor RUNTIME DESTINATION .)
install(FILES README.md DESTINATION .)
set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME Starter3DEngine)
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
include(CPack)
