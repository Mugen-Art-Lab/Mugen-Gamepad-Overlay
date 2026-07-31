cmake_minimum_required(VERSION 3.28...3.31)

# This CMakeLists.txt is designed to replace the root CMakeLists.txt in the
# official obsproject/obs-plugintemplate. The setup script does that for you.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version} LANGUAGES C CXX)

include(compilerconfig)
include(defaults)
include(helpers)

include(FetchContent)

# Mugen Gamepad Overlay only needs SDL's controller stack. Disabling unrelated
# multimedia subsystems makes first-time Windows builds substantially smaller
# and faster while keeping Gamepad, Joystick, HIDAPI, RawInput and XInput.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
set(SDL_VIDEO OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
set(SDL_RENDER OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_POWER OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_DIALOG OFF CACHE BOOL "" FORCE)
set(SDL_TRAY OFF CACHE BOOL "" FORCE)
set(SDL_JOYSTICK ON CACHE BOOL "" FORCE)
set(SDL_HIDAPI ON CACHE BOOL "" FORCE)
set(SDL_HIDAPI_JOYSTICK ON CACHE BOOL "" FORCE)
set(SDL_XINPUT ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.4.12
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL3)

# NanoSVG is used only to rasterize local SVG assets referenced by
# GamepadViewer CSS skins. Pin the source so builds remain reproducible.
FetchContent_Declare(
  nanosvg
  GIT_REPOSITORY https://github.com/memononen/nanosvg.git
  GIT_TAG 25241c5a8f8451d41ab1b02ab2d865b01600d949
)
FetchContent_MakeAvailable(nanosvg)

# OBS enables link-time code generation globally for release configurations.
# SDL deliberately disables /GL for SDL_mslibc.c because Microsoft's LTO
# cannot be used for its memcpy/memset implementation. SDL 3.4.12 also applies
# its precompiled header to that source, and MSVC 17.14 then reports C4652;
# the OBS template promotes that warning to an error. This one file must skip
# SDL's PCH so its intentional /GL- setting remains valid.
if(MSVC)
  set_source_files_properties(
    "${sdl3_SOURCE_DIR}/src/stdlib/SDL_mslibc.c"
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
  )
  set_property(TARGET SDL3-static PROPERTY COMPILE_WARNING_AS_ERROR OFF)
endif()

add_library(${CMAKE_PROJECT_NAME} MODULE)
find_package(libobs REQUIRED)

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/gamepad_source.cpp
    src/gamepad_source.hpp
    src/gamepadviewer_skin.cpp
    src/gamepadviewer_skin.hpp
    src/svg_rasterizer.cpp
    src/svg_rasterizer.hpp
    src/input_types.hpp
    src/universal_skin.cpp
    src/universal_skin.hpp
    src/input_overlay_preset.cpp
    src/input_overlay_preset.hpp
    src/preset_variants.cpp
    src/preset_variants.hpp
    src/skin.cpp
    src/skin.hpp
    src/pixel_canvas.cpp
    src/pixel_canvas.hpp
)

target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_20)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE SDL_MAIN_HANDLED=1)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${nanosvg_SOURCE_DIR}/src")
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs SDL3::SDL3-static)

if(MSVC)
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /W4 /permissive- /EHsc /utf-8)
  # NanoSVG expands its C implementation inside this translation unit. Keep
  # our project at /W4 while suppressing third-party-header warnings there.
  set_source_files_properties(src/svg_rasterizer.cpp PROPERTIES COMPILE_OPTIONS "/W0")
else()
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
