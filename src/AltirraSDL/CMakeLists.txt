cmake_minimum_required(VERSION 3.24)

# Collect all Altirra simulator source files
file(GLOB ALTIRRA_ALL_SOURCES
    "${CMAKE_SOURCE_DIR}/src/Altirra/source/*.cpp"
)

# Filter out Win32 UI, entry point, and platform-specific files
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/ui[a-zA-Z][^/]*\\.cpp$")
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*_win32\\.cpp$")
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/oshelper\\.cpp$")
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/console\\.cpp$")
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/videowriter\\.cpp$")
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/aviwriter\\.cpp$")
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/directorywatcher\\.cpp$")
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/joystick\\.cpp$")
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/safewrite\\.cpp$")
# Exclude files that use Win32-only APIs and have no emulation purpose
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/about\\.cpp$")       # richedit.h
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/allocator\\.cpp$")   # crtdbg.h (MSVC debug heap)
# Exclude UI command handler files (include atnativeui/uiframe.h -> windows.h)
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/cmd[a-z][^/]*\\.cpp$")
# Exclude files with Win32 UI dependencies not needed for emulation core
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/debugger\\.cpp$")          # includes atnativeui/uiframe.h
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/customdevice\\.cpp$")      # includes customdevice_win32.h
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/customdevicevmtypes\\.cpp$") # includes atui/uicommandmanager.h
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/cpuheatmap\\.cpp$")        # Win32 UI visualization
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/cs8900a\\.cpp$")           # Win32 UI debug path
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/debuggerautotest\\.cpp$")  # includes atui/uicommandmanager.h
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/idephysdisk\\.cpp$")       # Win32 raw disk access (DeviceIoControl)
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/idevhdimage\\.cpp$")       # requires sizeof(wchar_t)==2 (UTF-16)
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/midimate\\.cpp$")          # Win32 MIDI API (winmm.h)
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/modemtcp\\.cpp$")          # Win32 networking (windows.h)
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/profilerui\\.cpp$")        # Win32 profiler UI (w32assist.h)
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/settings\\.cpp$")          # Win32 UI accessors (uiportmenus.h -> windows.h)
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/startuplogger\\.cpp$")     # Win32 only (windows.h)
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/texteditor\\.cpp$")        # Win32 HWND/WndProc text editor widget
list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*/vgmwriter\\.cpp$")         # requires sizeof(wchar_t)==2 (UTF-16 VGM metadata)
# Exclude ARM-only files on x86_64
if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "(aarch64|ARM64)")
    list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*_neon\\.cpp$")
    list(FILTER ALTIRRA_ALL_SOURCES EXCLUDE REGEX ".*_arm64\\.cpp$")
endif()

add_executable(AltirraSDL
    # SDL3 frontend
    source/main_sdl3.cpp
    source/display_sdl3.cpp
    source/input_sdl3.cpp

    # Dear ImGui UI
    source/ui_main.cpp

    # Stub implementations replacing Win32-only files
    stubs/console_stubs.cpp
    stubs/oshelper_stubs.cpp
    stubs/joystick_stubs.cpp
    stubs/uirender_stubs.cpp
    stubs/win32_stubs.cpp
    stubs/device_stubs.cpp

    # Audio output
    ${CMAKE_SOURCE_DIR}/src/ATAudio/source/audiooutput_sdl3.cpp

    # All emulation core files (filtered above)
    ${ALTIRRA_ALL_SOURCES}
)

target_include_directories(AltirraSDL PRIVATE
    # Public system headers
    ${ALTIRRA_INCLUDE_DIR}
    # Altirra private headers (simulator.h, gtia.h, etc.)
    ${CMAKE_SOURCE_DIR}/src/Altirra/h
    # Altirra source dir (for #include "foo.h" without path)
    ${CMAKE_SOURCE_DIR}/src/Altirra/source
    # Stubs directory (overrides Win32 headers with portability stubs)
    ${CMAKE_SOURCE_DIR}/src/AltirraSDL/stubs
    # SDL3 frontend headers
    ${CMAKE_SOURCE_DIR}/src/AltirraSDL/source
    # ATAudio headers
    ${CMAKE_SOURCE_DIR}/src/ATAudio/h
    # Resource ID constants (#define IDD_*, IDR_*, etc.)
    ${CMAKE_SOURCE_DIR}/src/Altirra/res
    # Auto-generated binary blobs (pbidisk.inl, etc.)
    ${CMAKE_SOURCE_DIR}/src/Altirra/autogen
    # Default version.h for local/cross-platform builds
    ${CMAKE_SOURCE_DIR}/src/Altirra/autobuild_default
    # Compat shims (intrin.h, tchar.h)
    ${ALTIRRA_COMPAT_DIR}
)

# Altirra source precompiled header (each .cpp does #include <stdafx.h>)
# Point to the Altirra project's own stdafx.h
target_include_directories(AltirraSDL BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Altirra/h
)

target_link_libraries(AltirraSDL PRIVATE
    # Core emulation libraries
    ATCPU
    ATEmulation
    ATDevices
    ATIO
    ATAudio
    ATNetwork
    ATDebugger
    ATVM
    ATCore
    Kasumi
    vdjson
    system

    # SDL3
    SDL3::SDL3

    # Dear ImGui
    imgui
)

target_compile_definitions(AltirraSDL PRIVATE
    VD_OS_LINUX=1
)

# Per-file SIMD flags for GCC
if(NOT MSVC AND CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|AMD64)")
    # antic.cpp includes antic_sse2.inl which uses _tzcnt_u32 (BMI1)
    set_source_files_properties(
        "${CMAKE_SOURCE_DIR}/src/Altirra/source/antic.cpp"
        PROPERTIES COMPILE_FLAGS "-mbmi"
    )
    # tracefileencoding.cpp uses SSSE3 + popcnt intrinsics
    set_source_files_properties(
        "${CMAKE_SOURCE_DIR}/src/Altirra/source/tracefileencoding.cpp"
        PROPERTIES COMPILE_FLAGS "-mssse3 -mpopcnt"
    )
endif()
