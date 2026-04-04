#!/usr/bin/env bash
#
# build.sh — Cross-platform build script for AltirraSDL
#
# Usage:
#   ./build.sh                    Build release for current platform
#   ./build.sh --debug            Build debug
#   ./build.sh --package          Build + create distributable archive
#   ./build.sh --package --source Also create source archive
#   ./build.sh --clean            Clean rebuild
#   ./build.sh --native           Windows only: build libs for .sln (no SDL3)
#   ./build.sh --jobs 8           Override parallel job count
#   ./build.sh --cmake "-DFOO=1"  Pass extra CMake arguments
#   ./build.sh --help             Show this help
#
# On Windows, run from Git Bash, MSYS2, or WSL.
# Requires: cmake 3.24+, C++20 compiler, SDL3 dev package.
#
# Output archives (with --package):
#   build/<preset>/AltirraSDL-<ver>-<platform>.zip
#   build/<preset>/AltirraSDL-<ver>-src.tar.gz  (with --source)

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
SCRIPTS_DIR="$ROOT_DIR/scripts/build"

source "$SCRIPTS_DIR/common.sh"

# ── Defaults ──────────────────────────────────────────────────────────────
BUILD_TYPE=release
FRONTEND=sdl
CLEAN=0
PACKAGE=0
SOURCE_ARCHIVE=0
JOBS=""
CMAKE_EXTRA_ARGS=""

# ── Parse arguments ───────────────────────────────────────────────────────
while [ $# -gt 0 ]; do
    case "$1" in
        --debug)    BUILD_TYPE=debug ;;
        --release)  BUILD_TYPE=release ;;
        --native)   FRONTEND=native ;;
        --sdl)      FRONTEND=sdl ;;
        --clean)    CLEAN=1 ;;
        --package)  PACKAGE=1 ;;
        --source)   SOURCE_ARCHIVE=1 ;;
        --jobs)     shift; JOBS="$1" ;;
        -j*)        JOBS="${1#-j}" ;;
        --cmake)    shift; CMAKE_EXTRA_ARGS="$1" ;;
        --help|-h)
            sed -n '3,/^$/{ s/^# //; s/^#//; p }' "$0"
            exit 0 ;;
        *) die "Unknown option: $1  (try --help)" ;;
    esac
    shift
done

# ── Detect & resolve ──────────────────────────────────────────────────────
detect_platform
resolve_preset
detect_jobs

echo ""
info "Platform:   ${C_BOLD}${PLATFORM}${C_RESET}"
info "Build type: ${C_BOLD}${BUILD_TYPE}${C_RESET}"
info "Frontend:   ${C_BOLD}${FRONTEND}${C_RESET}"
info "Preset:     ${C_BOLD}${PRESET}${C_RESET}"
info "Jobs:       ${C_BOLD}${JOBS}${C_RESET}"
echo ""

# ── Export for sub-scripts ────────────────────────────────────────────────
export ROOT_DIR BUILD_DIR PRESET BUILD_TYPE PLATFORM FRONTEND
export CLEAN JOBS CMAKE_EXTRA_ARGS SOURCE_ARCHIVE

# ── Configure ─────────────────────────────────────────────────────────────
source "$SCRIPTS_DIR/configure.sh"

# ── Build ─────────────────────────────────────────────────────────────────
source "$SCRIPTS_DIR/compile.sh"

# ── Package (optional) ────────────────────────────────────────────────────
if [ "$PACKAGE" = "1" ]; then
    source "$SCRIPTS_DIR/package.sh"
fi

echo ""
ok "All done!"
