#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tmp_root="$(mktemp -d)"
trap 'rm -rf "$tmp_root"' EXIT

tmp_bin="$tmp_root/bin"
mkdir -p "$tmp_bin"
ln -s /usr/bin/bash "$tmp_bin/bash"
ln -s /usr/bin/dirname "$tmp_bin/dirname"
ln -s /usr/bin/env "$tmp_bin/env"

scripts=(
  "$repo_root/tests/check_securestore_build_policy.sh"
  "$repo_root/tests/check_windows_ninja_x64_policy.sh"
  "$repo_root/tests/check_ci_cmake_language_policy.sh"
)

python_path="$(resolve_test_python)"

for script in "${scripts[@]}"; do
  (cd "$repo_root" && PATH="$tmp_bin" PERASTAGE_TEST_PYTHON="$python_path" /usr/bin/bash "$script")
  (cd "$tmp_root" && PATH="$tmp_bin" PERASTAGE_TEST_PYTHON="$python_path" /usr/bin/bash "$script")
done

if PATH="$tmp_bin" command -v rg >/dev/null 2>&1; then
  echo 'Regression setup error: rg is unexpectedly available on PATH.' >&2
  exit 1
fi

echo 'OK: release-gate policy scripts are independent of rg and caller working directory.'
