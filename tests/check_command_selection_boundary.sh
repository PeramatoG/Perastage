#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
files=("$root/core/command/command_selection.h"
       "$root/core/command/command_selection.cpp"
       "$root/core/command/command_selection_text_adapter.h"
       "$root/core/command/command_selection_text_adapter.cpp")

if rg -ni '#include .*?(gui/|wx/|configmanager|mainwindow|viewer|tablepanel)' "${files[@]}"; then
  echo "Command selection code must remain independent of GUI and project singletons." >&2
  exit 1
fi

selection_block="$(sed -n '/add_library(perastage_command_selection STATIC/,/^)/p;/target_.*(perastage_command_selection/,/^)/p' "$root/core/CMakeLists.txt")"
if rg -ni '(wxwidgets|gui|app|mainwindow|configmanager|tablepanel|viewer2d|viewer3d|console)' <<<"$selection_block"; then
  echo "Semantic selection target has a forbidden frontend dependency." >&2
  exit 1
fi
if ! rg -q 'perastage_command_core' <<<"$selection_block"; then
  echo "Semantic selection target must depend on Command Core." >&2
  exit 1
fi

test_block="$(sed -n '/add_executable(command_selection_test/,/set_perastage_test_labels(CommandSelection/p' "$root/tests/CMakeLists.txt")"
if rg -n '(command_selection(_text_adapter)?\.cpp|\.\./(core|models)/.*\.cpp)' <<<"$test_block"; then
  echo "CommandSelection must link production targets instead of recompiling sources." >&2
  exit 1
fi

echo "Command selection boundary check passed."
