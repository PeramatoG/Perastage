#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

files=()
while IFS= read -r file; do
  files+=("$file")
done < <(find core/command -maxdepth 1 -type f -print | sort)

forbidden='(wx[A-Za-z]*|MainWindow|ConfigManager|IGui|[Tt]able[Pp]anel|Viewer2D|Viewer3D|["<](gui|app)/)'
if rg -n "$forbidden" "${files[@]}"; then
  echo "Command Core must remain independent of GUI and application services." >&2
  exit 1
fi

core_block="$(sed -n '/add_library(perastage_command_core STATIC/,/^)/p' core/CMakeLists.txt)"
if printf '%s\n' "$core_block" | rg -n '(wx|gui|app|ConfigManager)'; then
  echo "perastage_command_core has a forbidden build dependency." >&2
  exit 1
fi

echo "OK: Command Core remains GUI-independent."
