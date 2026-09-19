#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
files=(
  core/inspection/inspection_contract.h
  core/inspection/inspection_contract.cpp
)

cd "$repo_root"

validate_includes() {
  local file="$1"
  shift
  local include
  local allowed
  local accepted

  while IFS= read -r include; do
    accepted=false
    for allowed in "$@"; do
      if [[ "$include" == "$allowed" ]]; then
        accepted=true
        break
      fi
    done
    if [[ "$accepted" != true ]]; then
      echo "Unexpected include in $file: $include" >&2
      return 1
    fi
  done < <(
    rg -o '^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"][^>"]+[>"]' "$file" |
      sed -E 's/^[[:space:]]*#[[:space:]]*include[[:space:]]*//'
  )
}

# Every permitted dependency is explicit so a new project include requires review.
validate_includes core/inspection/inspection_contract.h \
  '<cstdint>' '<filesystem>' '<optional>' '<string>' '<vector>'
validate_includes core/inspection/inspection_contract.cpp \
  '"inspection_contract.h"' '<algorithm>'

forbidden='(wx[A-Z]|MainWindow|ConfigManager|ConsolePanel|Viewer[23]D|gettext|_\()'
if rg -n "$forbidden" "${files[@]}"; then
  echo "Core inspection contract must remain independent of GUI and application services." >&2
  exit 1
fi

echo "OK: Core inspection contract remains GUI-independent."
