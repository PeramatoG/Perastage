#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

make_portable_temp_dir() {
  local base="${TMPDIR:-${TEMP:-${TMP:-}}}"
  if [[ -z "$base" ]]; then
    base="$(pwd)"
  fi
  mkdir -p "$base"
  mktemp -d "$base/perastage-release-gate-policy.XXXXXX"
}

write_tool_wrapper() {
  local tool_name="$1"
  local source_path
  source_path="$(command -v "$tool_name")" || {
    echo "Required portability tool is not available: $tool_name" >&2
    exit 127
  }
  local target_name="$tool_name"
  case "$source_path" in
    *.exe) target_name="$tool_name.exe" ;;
  esac
  # Wrapping preserves platform-managed tool behavior that may be lost when its executable is copied on macOS.
  printf 'exec %q "$@"\n' "$source_path" >"$tmp_bin/$target_name"
  chmod +x "$tmp_bin/$target_name"
}

bash_path="${PERASTAGE_TEST_BASH:-${BASH:-}}"
if [[ -z "$bash_path" || ! -x "$bash_path" ]]; then
  bash_path="$(command -v bash)" || {
    echo 'Required portability tool is not available: bash' >&2
    exit 127
  }
fi
python_path="$(resolve_test_python)"
python_path="$("$python_path" - <<'PY_RESOLVE'
import sys
print(sys.executable)
PY_RESOLVE
)"
tmp_root="$(make_portable_temp_dir)"
trap 'rm -rf "$tmp_root"' EXIT

tmp_bin="$tmp_root/bin"
mkdir -p "$tmp_bin"
write_tool_wrapper dirname

expected_tests_dir="$repo_root/tests"
resolved_tests_dir="$(PATH="$tmp_bin" "$bash_path" -c 'dirname "$1"' _ "$repo_root/tests/check_securestore_build_policy.sh")"
if [[ "$resolved_tests_dir" != "$expected_tests_dir" ]]; then
  echo "Restricted-PATH dirname resolved '$resolved_tests_dir'; expected '$expected_tests_dir'." >&2
  exit 1
fi

scripts=(
  "$repo_root/tests/check_securestore_build_policy.sh"
  "$repo_root/tests/check_windows_ninja_x64_policy.sh"
  "$repo_root/tests/check_ci_cmake_language_policy.sh"
)

for script in "${scripts[@]}"; do
  script_name="$(basename "$script")"
  start_seconds="$SECONDS"
  (cd "$repo_root" && PATH="$tmp_bin" PERASTAGE_TEST_PYTHON="$python_path" "$bash_path" "$script")
  root_elapsed=$((SECONDS - start_seconds))
  echo "${script_name} from repository root completed in ${root_elapsed}s."
  start_seconds="$SECONDS"
  (cd "$tmp_root" && PATH="$tmp_bin" PERASTAGE_TEST_PYTHON="$python_path" "$bash_path" "$script")
  unrelated_elapsed=$((SECONDS - start_seconds))
  echo "${script_name} from unrelated working directory completed in ${unrelated_elapsed}s."
done

if PATH="$tmp_bin" command -v rg >/dev/null 2>&1; then
  echo 'Regression setup error: rg is unexpectedly available on PATH.' >&2
  exit 1
fi

echo 'OK: release-gate policy scripts are independent of rg and caller working directory.'
