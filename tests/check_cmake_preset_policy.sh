#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
require_ripgrep

run_test_python - <<'PY'
import json
from pathlib import Path

presets = json.loads(Path("CMakePresets.json").read_text(encoding="utf-8"))
configure = {preset["name"]: preset for preset in presets["configurePresets"]}
build = {preset["name"]: preset for preset in presets["buildPresets"]}

expected_configure = {
    "mac-arm64-debug": "Darwin",
    "mac-arm64-release": "Darwin",
    "wsl-x64-debug": "Linux",
    "wsl-x64-release": "Linux",
    "win-x64-debug-ninja": "Windows",
    "win-x64-release-ninja": "Windows",
}
expected_build = {
    "mac-debug-build": "mac-arm64-debug",
    "mac-release-build": "mac-arm64-release",
    "wsl-debug-build": "wsl-x64-debug",
    "wsl-release-build": "wsl-x64-release",
    "win-debug-build-ninja": "win-x64-debug-ninja",
    "win-release-build-ninja": "win-x64-release-ninja",
}

for name, host in expected_configure.items():
    preset = configure[name]
    assert preset["generator"] == "Ninja", name
    assert preset["condition"] == {
        "type": "equals",
        "lhs": "${hostSystemName}",
        "rhs": host,
    }, name

for name, configure_name in expected_build.items():
    assert build[name]["configurePreset"] == configure_name, name

for name in ("wsl-x64-debug", "wsl-x64-release"):
    cache = configure[name]["cacheVariables"]
    assert "/mnt/c/vcpkg" in cache["CMAKE_IGNORE_PREFIX_PATH"], name
    assert "/mnt/c/vcpkg" in cache["CMAKE_IGNORE_PATH"], name

for name in ("mac-arm64-debug", "mac-arm64-release"):
    cache = configure[name]["cacheVariables"]
    assert cache["CMAKE_TOOLCHAIN_FILE"] == "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake", name
    assert cache["CMAKE_OSX_ARCHITECTURES"] == "arm64", name
    assert cache["PERASTAGE_REQUIRE_SECURE_CREDENTIAL_STORE"] == "ON", name

for name in ("win-x64-debug-ninja", "win-x64-release-ninja"):
    preset = configure[name]
    cache = preset["cacheVariables"]
    assert preset["toolchainFile"] == "${sourceDir}/cmake/PerastageWindowsVcpkgToolchain.cmake", name
    assert "CMAKE_TOOLCHAIN_FILE" not in cache, name

setup_linux = Path("scripts/linux/PerastageLinuxBootstrap.sh").read_text(encoding="utf-8")
for name in ("wsl-x64-debug", "wsl-x64-release", "wsl-debug-build", "wsl-release-build"):
    assert name in setup_linux, name
assert 'cmake --preset "$configure_preset"' in setup_linux
assert 'cmake --build --preset "$build_preset"' in setup_linux

setup_windows = Path("scripts/windows/PerastageWindowsBootstrap.ps1").read_text(encoding="utf-8")
for name in ("win-x64-debug-ninja", "win-x64-release-ninja", "win-debug-build-ninja", "win-release-build-ninja"):
    assert name in setup_windows, name
assert "cmake --preset $ConfigurePreset" in setup_windows
assert "cmake --build --preset $BuildPreset" in setup_windows
PY

git check-ignore --quiet CMakeUserPresets.json
test -z "$(git ls-files -- CMakeUserPresets.json)"

if rg -n 'CMakeUserPresets\.json' \
    CMakeLists.txt CMakePresets.json setup.sh setup_windows.ps1 \
    scripts/linux scripts/windows .github/workflows packaging; then
    echo "Operational setup, CI, or packaging configuration must not require CMakeUserPresets.json." >&2
    exit 1
fi

echo "CMake shared preset and optional user-preset policy checks passed."
