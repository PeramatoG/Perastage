#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
core_cmake="$root/core/CMakeLists.txt"
parser_files=(
  "$root/core/command/command_text_parser.h"
  "$root/core/command/command_text_parser.cpp"
)

if rg -ni '#include .*?(gui/|app/|configmanager|mainwindow|igui|viewer|tablepanel|mvr|gdtf)' "${parser_files[@]}"; then
  echo "Command text parser must remain independent of GUI, App, scene IO, and viewers." >&2
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

parser_target_configuration="$(extract_target_configuration perastage_command_text_parser)"
if [[ -z "$parser_target_configuration" ]]; then
  echo "Command text parser target configuration was not found." >&2
  exit 1
fi
if rg -ni '(wxwidgets|(^|[^a-z])wx([^a-z]|$)|gui|app|mainwindow|configmanager|igui|viewer2d|viewer3d|tablepanel|mvr|gdtf|command_transform|scene_grouping|scene_node_operations)' \
    <<<"$parser_target_configuration"; then
  echo "Command text parser target has a forbidden execution, scene, or frontend dependency." >&2
  exit 1
fi
if ! rg -q 'perastage_command_core' <<<"$parser_target_configuration"; then
  echo "Command text parser target must retain its neutral Command Core dependency." >&2
  exit 1
fi

if rg -n 'command_text_parser' "$root/core/command/command_contract."{h,cpp} \
    "$root/core/command/command_execution.h" \
    "$root/core/command/command_mutation_transaction."{h,cpp}; then
  echo "Semantic Command Core must not depend on the human text parser." >&2
  exit 1
fi

command_core_configuration="$(extract_target_configuration perastage_command_core)"
if rg -n 'perastage_command_text_parser' <<<"$command_core_configuration"; then
  echo "Semantic Command Core CMake configuration must not depend on the human text parser." >&2
  exit 1
fi

if rg -n 'command_text_parser\.cpp' "$root/tests/CMakeLists.txt"; then
  echo "Tests must link the production parser target instead of recompiling it." >&2
  exit 1
fi

echo "Command text parser boundary check passed."
