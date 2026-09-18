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

forbidden='(<wx/|wx[A-Z]|MainWindow|ConfigManager|ConsolePanel|Viewer[23]D|gettext|_\()'
if rg -n "$forbidden" "${files[@]}"; then
  echo "Core inspection contract must remain independent of GUI and application services." >&2
  exit 1
fi

echo "OK: Core inspection contract remains GUI-independent."
