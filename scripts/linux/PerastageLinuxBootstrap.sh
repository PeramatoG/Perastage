#!/usr/bin/env bash

set -euo pipefail

CONFIGURATION="Debug"
SKIP_DEPS=0
SKIP_BUILD=0

# Prints usage information.
print_usage() {
    cat <<'EOF'
Usage:
  ./setup.sh [Debug|Release] [--skip-deps] [--skip-build]

Examples:
  ./setup.sh
  ./setup.sh Release
  ./setup.sh Debug --skip-deps
  ./setup.sh Release --skip-build
EOF
}

# Parses command-line arguments.
parse_arguments() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            Debug|Release)
                CONFIGURATION="$1"
                shift
                ;;
            --skip-deps)
                SKIP_DEPS=1
                shift
                ;;
            --skip-build)
                SKIP_BUILD=1
                shift
                ;;
            -h|--help)
                print_usage
                exit 0
                ;;
            *)
                echo "Unknown argument: $1" >&2
                print_usage
                exit 1
                ;;
        esac
    done
}

# Returns the repository root based on this script location.
get_repository_root() {
    cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd
}

# Verifies that a required command is available in PATH.
assert_command_available() {
    local command_name="$1"

    if ! command -v "$command_name" >/dev/null 2>&1; then
        echo "Required command '$command_name' was not found in PATH." >&2
        exit 1
    fi
}

# Installs common Debian, Ubuntu, and WSL build dependencies when apt is available.
install_apt_dependencies() {
    sudo apt-get update
    sudo apt-get install -y         build-essential         cmake         ninja-build         git         curl         pkg-config         libgl1-mesa-dev         libglu1-mesa-dev         libglew-dev         libcurl4-openssl-dev         libtinyxml2-dev         libpodofo-dev         zlib1g-dev         libwxgtk3.2-dev
}

# Installs common Fedora build dependencies when dnf is available.
install_dnf_dependencies() {
    sudo dnf install -y         gcc         gcc-c++         make         cmake         ninja-build         git         curl         pkgconf-pkg-config         mesa-libGL-devel         mesa-libGLU-devel         glew-devel         libcurl-devel         tinyxml2-devel         podofo-devel         zlib-ng-compat-devel         wxGTK-devel
}

# Installs system dependencies for the detected Linux package manager.
install_system_dependencies() {
    if [[ "$SKIP_DEPS" -eq 1 ]]; then
        echo "Skipping system dependency installation."
        return
    fi

    if command -v apt-get >/dev/null 2>&1; then
        install_apt_dependencies
        return
    fi

    if command -v dnf >/dev/null 2>&1; then
        install_dnf_dependencies
        return
    fi

    echo "No supported package manager found." >&2
    echo "Install dependencies manually, then rerun this script with --skip-deps." >&2
    exit 1
}

# Resolves the CMake configure preset for the selected configuration.
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

# Configures and optionally builds Perastage using CMake presets.
configure_and_build() {
    local configure_preset="$1"
    local build_preset="$2"

    cmake --preset "$configure_preset"

    if [[ "$SKIP_BUILD" -eq 1 ]]; then
        echo "Skipping build step."
        return
    fi

    cmake --build --preset "$build_preset"
}

parse_arguments "$@"

REPOSITORY_ROOT="$(get_repository_root)"
cd "$REPOSITORY_ROOT"

assert_command_available "cmake"

install_system_dependencies

CONFIGURE_PRESET="$(get_configure_preset)"
BUILD_PRESET="$(get_build_preset)"

configure_and_build "$CONFIGURE_PRESET" "$BUILD_PRESET"

echo "Perastage Linux setup completed successfully."
