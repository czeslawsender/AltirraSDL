cmake_minimum_required(VERSION 3.24)
project(Altirra VERSION 4.40 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Match MSVC convention: _DEBUG in debug builds
if(NOT WIN32)
    add_compile_definitions($<$<CONFIG:Debug>:_DEBUG>)
endif()

# Platform detection -- used in #ifdef guards throughout headers
if(WIN32)
    add_compile_definitions(VD_OS_WINDOWS=1)
elseif(APPLE)
    add_compile_definitions(VD_OS_MACOS=1)
elseif(UNIX)
    add_compile_definitions(VD_OS_LINUX=1)
endif()

# Shared include paths
set(ALTIRRA_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/src/h)

# SDL3 is required for non-Windows platform implementations
if(NOT WIN32)
    find_package(SDL3 REQUIRED)
endif()

# Compatibility shims for non-Windows (intercepts bare <intrin.h>, <tchar.h>)
if(NOT WIN32)
    set(ALTIRRA_COMPAT_DIR ${CMAKE_SOURCE_DIR}/src/compat)
endif()

# Dear ImGui (docking branch) — fetched automatically, built as static library
if(NOT WIN32)
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.91.8-docking
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(imgui)

    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
    )
    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui PUBLIC SDL3::SDL3)
    # Suppress warnings in third-party code
    target_compile_options(imgui PRIVATE -w)
endif()

add_subdirectory(src)
