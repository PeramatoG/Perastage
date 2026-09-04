#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_test_tool cmake
require_ripgrep

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fixture="$repo_root/tests/check_windows_vcpkg_resolver_fixture.cmake"
temporary_root="$(mktemp -d)"
trap 'rm -rf "$temporary_root"' EXIT
external_root="$temporary_root/external-vcpkg"
user_config_root="$temporary_root/user-config"
stale_config_root="$temporary_root/stale-config"
bundled_config_root="$temporary_root/bundled-config"

mkdir -p "$external_root/scripts/buildsystems" "$user_config_root/vcpkg" \
    "$stale_config_root/vcpkg" "$bundled_config_root/vcpkg"
touch "$external_root/.vcpkg-root" "$external_root/vcpkg.exe"
cat >"$external_root/scripts/buildsystems/vcpkg.cmake" <<'EOF'
set(SYNTHETIC_TOOLCHAIN_INCLUDED TRUE)
EOF
printf '%s\n' "$external_root" >"$user_config_root/vcpkg/vcpkg.path.txt"
printf '%s\n' "$temporary_root/missing-vcpkg" >"$stale_config_root/vcpkg/vcpkg.path.txt"
printf '%s\n' 'C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg' \
    >"$bundled_config_root/vcpkg/vcpkg.path.txt"

run_case() {
    local case_name="$1"
    cmake -DREPO_ROOT="$repo_root" \
        -DEXTERNAL_ROOT="$external_root" \
        -DUSER_CONFIG_ROOT="$user_config_root" \
        -DSTALE_CONFIG_ROOT="$stale_config_root" \
        -DBUNDLED_CONFIG_ROOT="$bundled_config_root" \
        -DRESULT_FILE="$temporary_root/$case_name.result" \
        -DCASE="$case_name" -P "$fixture" \
        >"$temporary_root/$case_name.log" 2>&1
}

for case_name in explicit-external visual-studio-override user-wide-only appdata-only; do
    if ! run_case "$case_name"; then
        echo "Resolver success fixture failed: $case_name" >&2
        cat "$temporary_root/$case_name.log" >&2
        exit 1
    fi
    if [[ "$(cat "$temporary_root/$case_name.result")" != 'PASS' ]]; then
        echo "Resolver success fixture did not write PASS: $case_name" >&2
        exit 1
    fi
done

if ! run_case bootstrap-user-wide; then
    echo 'Resolver bootstrap fixture failed: bootstrap-user-wide' >&2
    cat "$temporary_root/bootstrap-user-wide.log" >&2
    exit 1
fi
if [[ "$(cat "$temporary_root/bootstrap-user-wide.result")" != 'PASS' ]]; then
    echo 'Resolver bootstrap fixture did not write PASS: bootstrap-user-wide' >&2
    exit 1
fi

for case_name in stale-descriptor both-missing bundled-descriptor; do
    if run_case "$case_name"; then
        echo "Resolver failure fixture unexpectedly passed: $case_name" >&2
        exit 1
    fi
    rg -q 'could not resolve an external classic vcpkg checkout' "$temporary_root/$case_name.log"
    rg -q 'vcpkg.exe integrate install' "$temporary_root/$case_name.log"
done

echo 'OK: Windows vcpkg resolver and bootstrap select only valid external classic checkouts.'
