#!/usr/bin/env bash
set -euo pipefail

require_test_tool() {
  local tool="$1"
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "ERROR: required test tool '$tool' is not available on PATH." >&2
    exit 127
  fi
}

require_ripgrep() {
  require_test_tool rg
}

resolve_test_python() {
  if [[ -z "${PERASTAGE_TEST_PYTHON:-}" ]]; then
    echo "ERROR: PERASTAGE_TEST_PYTHON is not set by the test harness." >&2
    exit 127
  fi
  local python_path="$PERASTAGE_TEST_PYTHON"
  if [[ "$python_path" == *:* ]] && command -v cygpath >/dev/null 2>&1; then
    python_path="$(cygpath -u "$python_path")"
  fi
  if [[ ! -x "$python_path" ]]; then
    echo "ERROR: PERASTAGE_TEST_PYTHON does not point to an executable interpreter: $PERASTAGE_TEST_PYTHON" >&2
    exit 127
  fi
  printf '%s\n' "$python_path"
}

run_test_python() {
  local python_path
  python_path="$(resolve_test_python)"
  "$python_path" "$@"
}
