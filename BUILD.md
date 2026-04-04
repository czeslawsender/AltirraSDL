# Building AltirraSDL

Altirra supports two independent build paths:

| Build Path | Platform | Frontend | Build System |
|------------|----------|----------|--------------|
| **Visual Studio** | Windows | Native Win32 UI (Altirra.exe) | `.sln` |
| **CMake** | Linux, macOS, Windows | SDL3 + Dear ImGui (AltirraSDL) | CMake + `build.sh` |

Both paths coexist in the same repository and do not conflict (different
output directories: `.sln` uses `out/`, CMake uses `build/`).

---

## Quick Start (build.sh)

The `build.sh` script automates the CMake workflow on all platforms.

```bash
# Build release for current platform
./build.sh

# Build debug
./build.sh --debug

# Build + create distributable archive
./build.sh --package

# Build + binary archive + source archive
./build.sh --package --source

# Clean rebuild
./build.sh --clean --package
```

**On Windows**, run from **Git Bash**, **MSYS2**, or **WSL**.

### Output

| File | Contents |
|------|----------|
| `build/<preset>/src/AltirraSDL/AltirraSDL` | Executable |
| `build/<preset>/AltirraSDL-<ver>-<platform>.zip` | Binary distribution (with `--package`) |
| `build/<preset>/AltirraSDL-<ver>-src.tar.gz` | Source archive (with `--source`) |

The binary archive follows Altirra's distribution convention:
```
AltirraSDL-4.40-linux.zip
    AltirraSDL          (executable)
    Copying             (GPL v2+ license)
    extras/
        customeffects/  (shader/effect presets)
        sampledevices/  (custom device examples)
        deviceserver/   (Python scripts)
        readme.txt
```

### All Options

| Option | Description |
|--------|-------------|
| `--release` | Release build (default) |
| `--debug` | Debug build |
| `--package` | Create distributable archive after build |
| `--source` | Also create source archive (requires `--package`) |
| `--clean` | Remove build directory before configuring |
| `--native` | Windows only: build core libraries for use with Visual Studio `.sln` |
| `--jobs N` or `-jN` | Override parallel job count (default: all cores) |
| `--cmake "ARGS"` | Pass extra arguments to CMake configure |
| `--help` | Show help |

---

## Prerequisites

### Linux (Debian/Ubuntu)

```bash
sudo apt install cmake build-essential libsdl3-dev
```

### Linux (Fedora)

```bash
sudo dnf install cmake gcc-c++ SDL3-devel
```

### macOS

```bash
brew install cmake sdl3
```

### Windows (for CMake/SDL3 build)

```
vcpkg install sdl3:x64-windows
```

Or install SDL3 development libraries manually and ensure they are on
`CMAKE_PREFIX_PATH`.

### Windows (for Visual Studio native build)

- Visual Studio 2022 v17.14+ (v143 toolset)
- Windows 11 SDK (10.0.26100.0+)
- MADS 2.1.0+ (6502 assembler, for kernel ROM — optional)

Dear ImGui is fetched automatically via CMake FetchContent (no manual
install needed).

---

## CMake Build (manual)

If you prefer not to use `build.sh`, use CMake presets directly:

```bash
# Linux
cmake --preset linux-release
cmake --build build/linux-release -j$(nproc)
./build/linux-release/src/AltirraSDL/AltirraSDL

# macOS
cmake --preset macos-release
cmake --build build/macos-release -j$(sysctl -n hw.ncpu)

# Windows SDL (from Developer Command Prompt)
cmake --preset windows-sdl-release
cmake --build build/windows-sdl-release --config Release
```

### Available Presets

| Preset | Platform | Type |
|--------|----------|------|
| `linux-debug` | Linux | Debug |
| `linux-release` | Linux | Release |
| `macos-debug` | macOS | Debug |
| `macos-release` | macOS | Release |
| `windows-sdl-debug` | Windows | Debug (SDL3) |
| `windows-sdl-release` | Windows | Release (SDL3) |
| `windows-libs-only` | Windows | Core libraries only (no frontend) |

### Package Target

To create a distributable folder:

```bash
cmake --build build/linux-release --target package_altirra
# Creates: build/linux-release/AltirraSDL-4.40/
```

### Install Target

For system-wide installation (FHS layout):

```bash
cmake --install build/linux-release --prefix /usr/local
# Installs: /usr/local/bin/AltirraSDL
#           /usr/local/share/altirra/extras/
```

---

## Visual Studio Native Build (Windows)

The native Win32 build produces the traditional `Altirra.exe` with full
Win32 UI, Direct3D display, and WASAPI audio. This is the primary build
path on Windows and does not use CMake.

### Steps

1. Open `src/Altirra.sln` in Visual Studio 2022
2. Set startup project to **Altirra**
3. **First build must be Release x64** — this compiles the Asuka build tool
   used by other configurations
4. Then build any configuration:
   - **Debug** — unoptimized, full debug info
   - **Profile** — optimized with debug info
   - **Release** — fully optimized with LTCG

Output goes to `out/`. Intermediates to `obj/`.

### Solution Files

| Solution | Contents |
|----------|----------|
| `src/Altirra.sln` | Main emulator (32 projects) |
| `src/AltirraRMT.sln` | Raster Music Tracker plugins |
| `src/ATHelpFile.sln` | Help file (requires .NET 4.8, HTML Help 1.4) |

### Local Overrides

Place `.props` files in `localconfig/active/` to override build settings
without modifying tracked files. See `localconfig/example/` for templates.

MADS assembler path can be overridden via the `ATMadsPath` property in
`localconfig/active/Altirra.local.props`.

### Release Packaging (Windows native)

```bash
py release.py    # From VS Developer Command Prompt
```

Requires Python 3.10+, 7-zip, AdvanceCOMP. Produces:
- `Altirra-<ver>.zip` — Binary distribution
- `Altirra-<ver>-src.7z` — Source archive

---

## Using Both Build Paths

The Visual Studio `.sln` and CMake builds are fully independent:

| | Visual Studio | CMake |
|---|---|---|
| **Source directory** | `src/Altirra.sln` | `CMakeLists.txt` (root) |
| **Output** | `out/` | `build/<preset>/` |
| **Intermediates** | `obj/`, `lib/` | `build/<preset>/` |
| **Frontend** | Native Win32 | SDL3 + Dear ImGui |
| **Emulation core** | Same source files | Same source files |

On Windows, you can build both:
- Native `Altirra.exe` via Visual Studio
- SDL3 `AltirraSDL.exe` via `./build.sh` or CMake

The `windows-libs-only` preset builds just the core emulation libraries
via CMake, which can be useful for testing that the core compiles with
different compilers (GCC/Clang on Windows).

---

## Detailed Build Documentation

For internals (conditional compilation, SIMD selection, compatibility
shims, test mode), see [PORTING/BUILD.md](PORTING/BUILD.md).
