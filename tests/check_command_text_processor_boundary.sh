#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
core_cmake="$root/core/CMakeLists.txt"
processor_files=(
  "$root/core/command/command_text_processor.h"
  "$root/core/command/command_text_processor.cpp"
)

forbidden='(#[[:space:]]*include[[:space:]]*[<"]([^">]*/)?(gui|app|cli|viewer2d|viewer3d)/|wx[A-Z]|ConfigManager|MainWindow|TablePanel|Viewer[23]D|ConsolePanel|IPC|OSC|MCP)'
if rg -ni "$forbidden" "${processor_files[@]}"; then
  echo "Command text processing must remain independent of frontends, transports, and GUI state." >&2
  exit 1
fi

# Prints every CMake command whose first argument is the requested target.
extract_target_configuration() {
  local target="$1"
  awk -v target="$target" '
    function occurrences(text, character, copy) {
      copy = text
      return gsub(character, "", copy)
    }
    !capturing && $0 ~ "^[[:space:]]*[A-Za-z_][A-Za-z0-9_]*\\([[:space:]]*" target "([[:space:]]|$)" {
      capturing = 1
      depth = 0
    }
    capturing {
      print
      depth += occurrences($0, "[(]") - occurrences($0, "[)]")
      if (depth <= 0)
        capturing = 0
    }
  ' "$core_cmake"
}

processor_configuration="$(extract_target_configuration perastage_command_text_processor)"
if [[ -z "$processor_configuration" ]]; then
  echo "The focused command text processor production target is missing." >&2
  exit 1
fi
target_forbidden='(wxwidgets|(^|[^a-z])wx([^a-z]|$)|(^|[^a-z])(gui|app|cli)([^a-z]|$)|configmanager|mainwindow|tablepanel|viewer2d|viewer3d|inspector.*presentation|ipc|(^|[^a-z])osc([^a-z]|$)|(^|[^a-z])mcp([^a-z]|$)|remote.*transport|transport.*remote|ai.*adapter|voice.*adapter)'
if rg -ni "$target_forbidden" <<<"$processor_configuration"; then
  echo "Command text processor target has a forbidden frontend, application, or transport dependency." >&2
  exit 1
fi
for dependency in perastage_command_text_parser \
                  perastage_command_selection_text_adapter \
                  perastage_command_transform_text_adapter; do
  if ! rg -q "^[[:space:]]*$dependency([[:space:]]|$)" \
      <<<"$processor_configuration"; then
    echo "Command text processor target must retain dependency: $dependency" >&2
    exit 1
  fi
done

if rg -n '(selection::ExecuteClear|text::ExecuteSelection|transform::Execute|ParseCommandLine)' \
    "$root/gui/consolepanel.cpp"; then
  echo "ConsolePanel must delegate reusable parsing and semantic dispatch to the text processor." >&2
  exit 1
fi

if ! rg -q 'perastage_command_text_processor' "$root/gui/CMakeLists.txt"; then
  echo "The embedded Console must link the shared text processor." >&2
  exit 1
fi

echo "Command text processor boundary check passed."
