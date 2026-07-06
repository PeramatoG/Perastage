#!/usr/bin/env bash

set -euo pipefail

CONFIGURATION="${1:-Debug}"

# Returns the repository root based on this script location.
get_repository_root() {
    cd "$(dirname "${BASH_SOURCE[0]}")" && pwd
}

# Verifies that a required command is available in PATH.
assert_command_available() {
    local command_name="$1"

    if ! command -v "$command_name" >/dev/null 2>&1; then
        echo "Required command '$command_name' was not found in PATH." >&2
        exit 1
    fi
}

# Installs common Debian/Ubuntu/WSL build dependencies when apt is available.
install_apt_dependencies() {
    sudo apt-get update
    sudo apt-get install -y \
        build-essential \
        cmake \
        ninja-build \
        git \
        curl \
        pkg-config \
        libgl1-mesa-dev \
        libglu1-mesa-dev \
        libglew-dev \
        libcurl4-openssl-dev \
        libtinyxml2-dev \
        libpodofo-dev \
        zlib1g-dev \
        libwxgtk3.2-dev
}

# Installs common Fedora build dependencies when dnf is available.
install_dnf_dependencies() {
    sudo dnf install -y \
        gcc \
        gcc-c++ \
        make \
        cmake \
        ninja-build \
        git \
        curl \
        pkgconf-pkg-config \
        mesa-libGL-devel \
        mesa-libGLU-devel \
        glew-devel \
        libcurl-devel \
        tinyxml2-devel \
        podofo-devel \
        zlib-ng-compat-devel \
        wxGTK-devel
}

# Installs system dependencies for the detected Linux package manager.
install_system_dependencies() {
    if command -v apt-get >/dev/null 2>&1; then
        install_apt_dependencies
        return
    fi

    if command -v dnf >/dev/null 2>&1; then
        install_dnf_dependencies
        return
    fi

    echo "No supported package manager found. Install dependencies manually and rerun the CMake preset commands." >&2
}

# Resolves the CMake configure and build presets for the selected configuration.
get_configure_preset() {
    case "$CONFIGURATION" in
        Debug)
            echo "wsl-x64-debug"
            ;;
        Release)
            echo "wsl-x64-release"
            ;;
        *)
            echo "Unsupported configuration '$CONFIGURATION'. Use Debug or Release." >&2
            exit 1
            ;;
    esac
}

# Resolves the CMake build preset for the selected configuration.
get_build_preset() {
    case "$CONFIGURATION" in
        Debug)
            echo "wsl-debug-build"
            ;;
        Release)
            echo "wsl-release-build"
            ;;
        *)
            echo "Unsupported configuration '$CONFIGURATION'. Use Debug or Release." >&2
            exit 1
            ;;
    esac
}

REPOSITORY_ROOT="$(get_repository_root)"
cd "$REPOSITORY_ROOT"

assert_command_available "cmake"

install_system_dependencies

CONFIGURE_PRESET="$(get_configure_preset)"
BUILD_PRESET="$(get_build_preset)"

cmake --preset "$CONFIGURE_PRESET"
cmake --build --preset "$BUILD_PRESET"

echo "Perastage Linux setup completed successfully."
