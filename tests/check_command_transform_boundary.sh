#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
files=("$root/core/command/command_transform.h"
       "$root/core/command/command_transform.cpp")

if rg -ni '#include .*?(gui/|app/|configmanager|mainwindow|viewer|tablepanel|command_text_parser)' "${files[@]}"; then
  echo "Semantic transforms must remain independent of frontends and application state." >&2
  exit 1
fi
if rg -n 'command_transform\.cpp' "$root/tests/CMakeLists.txt"; then
  echo "Tests must link the production transform target instead of recompiling it." >&2
  exit 1
fi
if ! rg -q 'add_library\(perastage_command_transform' "$root/core/CMakeLists.txt"; then
  echo "The focused semantic transform production target is missing." >&2
  exit 1
fi

echo "Command transform boundary check passed."
