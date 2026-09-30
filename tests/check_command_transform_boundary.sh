#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
core_cmake="$root/core/CMakeLists.txt"
files=("$root/core/command/command_transform.h"
       "$root/core/command/command_transform.cpp")

if rg -ni '#include .*?(gui/|app/|configmanager|mainwindow|viewer|tablepanel|command_text_parser)' "${files[@]}"; then
  echo "Semantic transforms must remain independent of frontends and application state." >&2
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

transform_configuration="$(extract_target_configuration perastage_command_transform)"
if [[ -z "$transform_configuration" ]]; then
  echo "The focused semantic transform production target is missing." >&2
  exit 1
fi
if rg -ni '(wxwidgets|(^|[^a-z])wx([^a-z]|$)|gui|app|mainwindow|configmanager|tablepanel|viewer2d|viewer3d|console|command_text_parser)' \
    <<<"$transform_configuration"; then
  echo "Semantic transform target has a forbidden frontend, application, or text-parser dependency." >&2
  exit 1
fi
if ! rg -q 'perastage_command_core' <<<"$transform_configuration"; then
  echo "Semantic transform target must retain its Command Core dependency." >&2
  exit 1
fi

scene_support_configuration="$(extract_target_configuration perastage_mvr_core_support)"
scene_operations_configuration="$(extract_target_configuration perastage_scene_node_operations)"
if rg -q 'perastage_scene_node_operations' <<<"$scene_support_configuration"; then
  echo "Neutral MVR Core support must not inherit generic scene-node mutations." >&2
  exit 1
fi
if ! rg -q 'perastage_mvr_core_support' <<<"$scene_operations_configuration"; then
  echo "Scene-node operations must consume neutral MVR Core support." >&2
  exit 1
fi

# The test target lives in a separate CMake file, so inspect its bounded block.
command_test_configuration="$(awk '
  /add_executable\(command_transform_test([[:space:])]|$)/ { capture = 1 }
  capture { print }
  capture && /set_perastage_test_labels\(CommandTransform/ { labels = 1 }
  labels && /\)[[:space:]]*$/ { exit }
' "$root/tests/CMakeLists.txt")"
if rg -n '(command_transform(_text_adapter)?\.cpp|\.\./(core|models)/.*\.cpp)' \
    <<<"$command_test_configuration"; then
  echo "CommandTransform must link production targets instead of recompiling production sources." >&2
  exit 1
fi

echo "Command transform boundary check passed."
