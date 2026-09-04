#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_test_tool cmake
require_ripgrep

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fixture="$repo_root/tests/check_windows_vcpkg_guard_fixture.cmake"
temporary_root="$(mktemp -d)"
trap 'rm -rf "$temporary_root"' EXIT
mkdir -p "$temporary_root/vcpkg/scripts/buildsystems"
touch "$temporary_root/vcpkg/scripts/buildsystems/vcpkg.cmake"

run_case() {
    local case_name="$1"
    cmake -DREPO_ROOT="$repo_root" \
        -DVCPKG_ROOT_FIXTURE="$temporary_root/vcpkg" \
        -DCASE="$case_name" \
        -P "$fixture" >"$temporary_root/$case_name.log" 2>&1
}

if run_case missing-root; then
    echo 'Missing VCPKG_ROOT fixture unexpectedly passed.' >&2
    exit 1
fi
rg -q 'requires VCPKG_ROOT' "$temporary_root/missing-root.log"

if run_case bundled-visual-studio; then
    echo 'Visual Studio bundled-vcpkg fixture unexpectedly passed.' >&2
    exit 1
fi
rg -q "Visual Studio's bundled VC/vcpkg" "$temporary_root/bundled-visual-studio.log"
rg -q 'is not supported for this workflow' "$temporary_root/bundled-visual-studio.log"

if run_case mismatched-root; then
    echo 'Mismatched VCPKG_ROOT fixture unexpectedly passed.' >&2
    exit 1
fi
rg -q 'does not match the classic vcpkg' "$temporary_root/mismatched-root.log"

run_case valid-root
echo 'OK: Windows classic-vcpkg guard rejects missing, bundled, and mismatched toolchains.'
