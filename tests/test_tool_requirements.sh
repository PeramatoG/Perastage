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
