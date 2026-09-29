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

core_configuration="$(awk '
  /^[[:space:]]*(add_library|target_[[:alnum:]_]+)\(perastage_command_core([[:space:])]|$)/ {
    capture = 1
  }
  capture {
    print
  }
  capture && /\)[[:space:]]*$/ {
    capture = 0
  }
' core/CMakeLists.txt)"
cmake_forbidden='(wx|MainWindow|ConfigManager|IGui|Viewer2D|Viewer3D|(^|[/_:;-])(gui|app)([/_:;)-]|$))'
if printf '%s\n' "$core_configuration" | rg -ni "$cmake_forbidden"; then
  echo "perastage_command_core has a forbidden build dependency." >&2
  exit 1
fi

echo "OK: Command Core remains GUI-independent."
