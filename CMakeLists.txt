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

foreach(NAME IN ITEMS object.vert hud.vert hud.frag cull.comp debug.vert)
    add_custom_command(OUTPUT "${SHADER_DIR}/${NAME}.spv"
        COMMAND ${SHADER_COMMAND} "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/shaders/${NAME}" -o "${SHADER_DIR}/${NAME}.spv"
        DEPENDS "src/engine/shaders/${NAME}" VERBATIM)
endforeach()
add_custom_command(OUTPUT "${SHADER_DIR}/geometry_shaders.hpp"
    COMMAND "${CMAKE_COMMAND}" "-DINPUT_DIR=${SHADER_DIR}" "-DOUTPUT=${SHADER_DIR}/geometry_shaders.hpp"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_geometry_spirv.cmake"
    DEPENDS "${SHADER_DIR}/object.vert.spv" "${SHADER_DIR}/hud.vert.spv" "${SHADER_DIR}/hud.frag.spv"
        "${SHADER_DIR}/cull.comp.spv" "${SHADER_DIR}/debug.vert.spv" cmake/embed_geometry_spirv.cmake VERBATIM)

# Exactly one native implementation is linked into the shared engine.
if(WIN32)
    set(ENGINE_PLATFORM_SOURCE src/engine/windows.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(ENGINE_PLATFORM_SOURCE src/engine/linux.cpp)
else()
    message(FATAL_ERROR "Supported platforms: Windows and Linux")
endif()

set(ENGINE_ASM_DEFAULT OFF)
if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64|X86_64)$")
    set(ENGINE_ASM_DEFAULT ON)
endif()
option(ENGINE_USE_ASM "Use the NASM x86-64 platform ABI bridge" ${ENGINE_ASM_DEFAULT})
if(ENGINE_USE_ASM)
    if(NOT ENGINE_ASM_DEFAULT)
        message(FATAL_ERROR "call.asm requires an x86-64 target; use -DENGINE_USE_ASM=OFF")
    endif()
    if(WIN32)
        set(CMAKE_ASM_NASM_OBJECT_FORMAT win64)
    else()
        set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)
    endif()
    enable_language(ASM_NASM)
endif()

add_library(engine_core SHARED
    src/engine/objects.cpp
    src/engine/camera_controller.cpp
    "${SHADER_DIR}/geometry_shaders.hpp"
    src/engine/platform.cpp
    ${ENGINE_PLATFORM_SOURCE}
    src/engine/scene.cpp
    src/engine/software_renderer.cpp
    src/engine/log.cpp
    src/engine/ui/third_party/imgui/imgui.cpp
    src/engine/ui/third_party/imgui/imgui_draw.cpp
    src/engine/ui/third_party/imgui/imgui_tables.cpp
    src/engine/ui/third_party/imgui/imgui_widgets.cpp
    src/engine/ui/third_party/imgui/backends/imgui_impl_vulkan.cpp
    "${SHADER_DIR}/scene_shaders.hpp"
)
target_include_directories(engine_core PUBLIC include PRIVATE "${SHADER_DIR}")
# One ImGui implementation in the shared engine; consumers use the same ABI/config.
target_include_directories(engine_core SYSTEM PUBLIC src/engine/ui/third_party/imgui)
target_link_libraries(engine_core PRIVATE Vulkan::Vulkan)
target_compile_definitions(engine_core PRIVATE ENGINE_BUILD_DLL)
set_target_properties(engine_core PROPERTIES OUTPUT_NAME engine
    VERSION ${PROJECT_VERSION} SOVERSION 1)
if(ENGINE_USE_ASM)
    target_sources(engine_core PRIVATE src/engine/call.asm)
    target_compile_definitions(engine_core PRIVATE ENGINE_HAS_ASM)
    if(WIN32)
        target_sources(engine_core PRIVATE src/engine/engine_asm.def)
    endif()
endif()
if(WIN32)
    set_target_properties(engine_core PROPERTIES PREFIX "")
    target_sources(engine_core PRIVATE src/engine/ui/third_party/imgui/backends/imgui_impl_win32.cpp)
    target_link_libraries(engine_core PRIVATE user32 gdi32 dwmapi imm32)
else()
    option(ENGINE_LINUX_X11 "Build native Linux windows using X11/XWayland" ON)
    if(ENGINE_LINUX_X11)
        find_package(X11 REQUIRED)
        target_compile_definitions(engine_core PRIVATE ENGINE_HAS_X11)
        target_include_directories(engine_core SYSTEM PRIVATE ${X11_INCLUDE_DIR})
        target_link_libraries(engine_core PRIVATE ${X11_LIBRARIES})
    endif()
endif()
target_compile_features(engine_core PUBLIC cxx_std_20)

option(ENGINE_STATIC_MINGW_RUNTIME "Link MinGW runtimes into executables for redistribution" ON)
if(MINGW AND ENGINE_STATIC_MINGW_RUNTIME)
    # -static also selects libwinpthread.a if this GCC's libstdc++ needs it.
    # Vulkan stays an import library: vulkan-1.dll comes from the GPU driver.
    target_link_options(engine_core PUBLIC -static -static-libgcc -static-libstdc++)
endif()

if(MSVC)
    target_compile_options(engine_core PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/W4;/permissive-;/utf-8>")
else()
    target_compile_options(engine_core PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic>")
endif()

if(WIN32)
    add_executable(engine_editor WIN32
        src/editor/main.cpp
        src/editor/app.cpp
        src/editor/ui.cpp
        src/editor/render.cpp
        src/editor/scene_io.cpp
        src/editor/interaction.cpp
        src/editor/gizmo.cpp
        src/editor/window.cpp
    )
    target_link_libraries(engine_editor PRIVATE engine_core user32)
    target_compile_definitions(engine_editor PRIVATE UNICODE _UNICODE)
    if(MINGW)
        # MinGW needs this option to use the Unicode wWinMain entry point.
        target_link_options(engine_editor PRIVATE -municode)
    endif()
    if(MSVC)
        target_compile_options(engine_editor PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(engine_editor PRIVATE -Wall -Wextra -Wpedantic)
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
    add_executable(platform_tests tests/platform_tests.cpp)
    target_link_libraries(platform_tests PRIVATE engine_core)
    if(MSVC)
        target_compile_options(platform_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(platform_tests PRIVATE -UNDEBUG)
    endif()
    add_test(NAME platform_tests COMMAND platform_tests)
    add_executable(ui_tests tests/ui_tests.cpp)
    target_link_libraries(ui_tests PRIVATE engine_core)
    if(MSVC)
        target_compile_options(ui_tests PRIVATE /UNDEBUG /utf-8)
    else()
        target_compile_options(ui_tests PRIVATE -UNDEBUG)
    endif()
    add_test(NAME ui_tests COMMAND ui_tests)
    foreach(TEST IN ITEMS geometry_tests geometry_gpu_tests)
        add_executable(${TEST} tests/${TEST}.cpp)
        target_link_libraries(${TEST} PRIVATE engine_core)
        if(MSVC)
            target_compile_options(${TEST} PRIVATE /UNDEBUG /utf-8)
        else()
            target_compile_options(${TEST} PRIVATE -UNDEBUG)
        endif()
        add_test(NAME ${TEST} COMMAND ${TEST})
    endforeach()
    if(WIN32)
        # Exercise editor state without a window or Vulkan device. Building this
        # target also catches header/implementation drift in app.cpp.
        add_executable(editor_state_tests tests/editor_state_tests.cpp src/editor/app.cpp)
        target_include_directories(editor_state_tests PRIVATE src/editor)
        target_link_libraries(editor_state_tests PRIVATE engine_core user32)
        target_compile_definitions(editor_state_tests PRIVATE UNICODE _UNICODE)
        if(MSVC)
            target_compile_options(editor_state_tests PRIVATE /UNDEBUG /W4 /permissive- /utf-8)
        else()
            target_compile_options(editor_state_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME editor_state_tests COMMAND editor_state_tests)
    endif()
    if(MINGW AND ENGINE_STATIC_MINGW_RUNTIME AND CMAKE_OBJDUMP)
        # Unlike rendering tests, this check does not need a Vulkan driver/GPU.
        add_test(NAME engine_runtime_dependencies
            COMMAND "${CMAKE_COMMAND}" "-DEXECUTABLE=$<TARGET_FILE:engine_editor>"
                "-DOBJDUMP=${CMAKE_OBJDUMP}"
                "-DENGINE_LIBRARY=$<TARGET_FILE:engine_core>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_windows_runtime.cmake")
    endif()
endif()

# A small distributable ZIP: shaders are embedded, compiler runtimes are static
# by default with MinGW. Never package the developer's GPU driver/Vulkan SDK.
# DLL/.so must travel with the editor. Linux resolves the installed .so beside it.
set_target_properties(engine_editor PROPERTIES INSTALL_RPATH "$ORIGIN")
install(TARGETS engine_editor engine_core
    RUNTIME DESTINATION . LIBRARY DESTINATION . ARCHIVE DESTINATION lib)
install(FILES README.md DESTINATION .)
install(FILES docs/objects.md DESTINATION docs)
install(FILES src/engine/ui/third_party/imgui/LICENSE.txt DESTINATION licenses RENAME Dear-ImGui.txt)
set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME Starter3DEngine)
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
include(CPack)

option(ENGINE_BUILD_WINDOW_EXAMPLE "Build the cross-platform native-window example" OFF)
if(ENGINE_BUILD_WINDOW_EXAMPLE)
    add_executable(platform_window_example examples/platform_window.cpp)
    target_link_libraries(platform_window_example PRIVATE engine_core)
endif()
