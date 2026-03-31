cmake_minimum_required(VERSION 3.24)
project(Altirra VERSION 4.40 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

add_subdirectory(src)
