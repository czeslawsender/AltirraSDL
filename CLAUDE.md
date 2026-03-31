# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Altirra is an Atari 800/800XL/5200 emulator, authored by Avery Lee. Licensed under GNU GPL v2+. The codebase is ~520K lines of C++ with 6502 assembly for the kernel ROM.

The project supports two build paths:
- **Windows (primary):** Native Win32 UI, Direct3D display, built via Visual Studio `.sln`
- **Cross-platform (in progress):** SDL3 + Dear ImGui frontend, built via CMake, targeting Linux and macOS

See `PORTING/` for the full cross-platform implementation plan.

## Build System

### Windows (Visual Studio — primary)

**Requirements:** Windows 10 x64+, Visual Studio 2022 v17.14+ (v143 toolset), Windows 11 SDK (10.0.26100.0+), MADS 2.1.0+ (6502 assembler).

**Solution files:**
- `src/Altirra.sln` — Main emulator (32 projects)
- `src/AltirraRMT.sln` — Raster Music Tracker plugins
- `src/ATHelpFile.sln` — Help file (requires .NET 4.8, C++/CLI, HTML Help 1.4)

**Build steps:**
1. Open `src/Altirra.sln`, set startup project to `Altirra`
2. First build must be **Release x64** (or Release ARM64 on ARM) to compile build tools used by other configurations
3. Then build any configuration: Debug (unoptimized), Profile (optimized), Release (LTCG)

**Output:** `out/` directory. Intermediates in `obj/`, libraries in `lib/` (all deletable).

**Kernel ROM:** Built via `src/Kernel/Makefile` using MADS assembler. The kernel is 6502 assembly.

**Release builds:** `py release.py` from a VS Developer Command Prompt. Requires Python 3.10+, 7-zip, AdvanceCOMP.

**Local overrides:** Place `.props` files in `localconfig/active/` (see `localconfig/example/` for templates). MADS path can be overridden via `ATMadsPath` property in `localconfig/active/Altirra.local.props`.

### Cross-Platform (CMake — in progress)

**Requirements:** CMake 3.24+, C++17 compiler (GCC or Clang), SDL3, SDL3_net, Dear ImGui (vendored).

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
./src/AltirraSDL/AltirraSDL
```

The CMake build exists alongside the `.sln` — it does not replace it. See `PORTING/BUILD.md` for details.

## Code Style

- **Indentation:** Tabs for C++/headers/shaders (`.cpp`, `.h`, `.inl`, `.fx`, `.fxh`), spaces for other file types
- **Guideline width:** 80 columns
- See `src/.editorconfig` for per-filetype settings

## Architecture

The project is organized as ~32 Visual Studio projects under `src/`, each a distinct module:

### Core Emulation (platform-agnostic — zero Win32 API calls)
- **ATCPU** — 6502/65C02/65C816 CPU emulation (cycle-accurate)
- **ATEmulation** — Hardware emulation (ANTIC video, GTIA graphics, POKEY audio/IO, PIA)
- **ATDevices** — Peripheral device emulation (disk drives, cassette, serial, etc.)
- **ATIO** — Disk/tape image I/O and format handling
- **ATNetwork** — Emulated Atari network stack (IP, TCP, UDP)
- **ATDebugger** — Debugger backend (symbols, targets, breakpoints)
- **ATCompiler / ATVM** — Compiler infrastructure and virtual machine for BASIC
- **ATCore** — Core utilities, base classes, scheduler
- **Kasumi** — Image processing and blitting
- **vdjson** — JSON parser

### Platform-Dependent (Win32 on Windows, SDL3 replacements on other platforms)
- **ATAudio** — Audio synthesis (cross-platform) + output backends (platform-specific)
- **ATNetworkSockets** — OS socket adapter (Winsock on Windows, SDL3_net on others)
- **system** — System abstraction layer (threading, file I/O, filesystem, config)

### Windows-Only (not compiled for SDL3 build)
- **Altirra** — Main Win32 application (~225+ source files, includes UI)
- **ATNativeUI** — Windows native UI (Win32 HWND, dialogs, menus)
- **ATUI / ATUIControls** — Custom widget system (depends on VDDisplay)
- **VDDisplay** — Display rendering (GDI/D3D9 — 15+ Win32-dependent files)
- **Riza** — Win32 audio backends (WaveOut, DirectSound, XAudio2, WASAPI)
- **Dita** — Dialog toolkit (COM/shell APIs)
- **Tessa** — Win32-specific
- **ATAppBase** — Win32 application base framework
- **AltirraShell** — Shell integration (file associations)
- **Asuka** — Build tool (compiled first, used by other projects)

### SDL3 Frontend (new, cross-platform)
- **AltirraSDL** — SDL3 + Dear ImGui frontend (display, audio, input, UI)

### Software Emulation
- **Kernel** — Atari OS kernel ROM implementation in 6502 assembly (~97 .s/.xasm files)
- **ATBasic** — Atari BASIC ROM in 6502 assembly (.s files, built by MADS)

### Headers
Shared headers live in `src/h/` with two namespaces:
- `src/h/at/` — Altirra-specific headers (atcore, atcpu, atemulation, etc.)
- `src/h/vd2/` — Legacy VirtualDub2 library headers (Kasumi, Dita, Riza, Tessa, VDDisplay, system)

### Build Support
- **Build/** — MSBuild property sheets (`.props`/`.targets`)
- **Shared/** — Shared resources including `altirra.natvis` for Visual Studio debug visualization

## Cross-Platform Porting

Detailed documentation lives in `PORTING/`:

| Document | Contents |
|----------|----------|
| `OVERVIEW.md` | Architecture, design principles, 8-phase plan |
| `SYSTEM.md` | System library (threading, file I/O, filesystem, VDStringW/wchar_t, config) |
| `BUILD.md` | CMake structure, excluded libraries, simulator extraction |
| `DISPLAY.md` | SDL3 video display, frame submission protocol |
| `AUDIO.md` | SDL3 audio output via IATAudioOutput |
| `INPUT.md` | SDL3 event to ATInputCode translation |
| `UI.md` | Dear ImGui replacing ATNativeUI and ATUI |
| `NETWORK.md` | SDL3_net replacing Winsock in ATNetworkSockets |
| `MAIN_LOOP.md` | SDL3 main loop replacing Win32 message pump |

**Key design principles:**
- Minimal changes to existing files (`#ifdef` guards only where platform types appear in headers)
- New `_sdl3.cpp` files alongside existing Win32 `.cpp` files; build system selects which to compile
- The Windows `.sln` build is always preserved and must never break
- SDL3 is the sole platform layer for non-Windows (no raw POSIX, ALSA, etc.)
- Only two external dependencies: SDL3/SDL3_net and Dear ImGui

**Implementation phases:**
1. CMake build system (Windows validation)
2. Header cleanup (`#ifdef` guards)
3. System library SDL3 implementations
4. Simulator library extraction
5. Minimal SDL3 frontend (first pixels)
6. Dear ImGui UI
7. Debugger UI
8. Network and remaining features

## Testing

- **ATTest** — Internal C++ test framework (~1,600 lines)
- **AltirraTest** — Test harness project
- **ATBasic/tests/** — BASIC interpreter test suite (~25 `.txt` test files)

## Key Design Notes

- The Windows build uses Win32 API throughout with no external dependencies (statically linked, no redistributables needed).
- The VD2/VirtualDub2 libraries (Kasumi, Dita, Riza, Tessa, system) are legacy foundations; their headers use the `vd2/` namespace.
- The `system` library uses `VDStringW` (wchar_t) for all file paths. On non-Windows, `_sdl3.cpp` files convert to UTF-8 at the OS boundary using `VDTextWToU8()` / `VDTextU8ToW()` from `text.h`.
- Link errors during build usually mean an upstream project failed — always look for the first error.
- Core emulation libraries (ATCPU, ATEmulation, ATDevices, ATIO, ATNetwork, ATDebugger, ATCompiler, ATVM, Kasumi, vdjson) contain zero Win32 API calls and compile unchanged on any platform once system library headers are cleaned up.
