#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
files=("$root/core/command/command_scene_tools.h"
       "$root/core/command/command_scene_tools.cpp")
domain_files=("$root/core/scene_grouping.h"
              "$root/core/scene_grouping.cpp"
              "$root/core/scene_node_operations.h"
              "$root/core/scene_node_operations.cpp"
              "$root/core/scene_object_truss_converter.h"
              "$root/core/scene_object_truss_converter.cpp")

if rg -ni '#include [<"].*(wx/|configmanager|mainwindow|panel|viewer)|::Instance\(|singleton' "${files[@]}"; then
  echo "Scene-tool Command Core must remain independent from GUI and application-owned state." >&2
  exit 1
fi

if ! rg -q 'scene_grouping::GroupSelection' "$root/core/command/command_scene_tools.cpp" ||
   ! rg -q 'scene_node_operations::ConvertFixtureToSupport' "$root/core/command/command_scene_tools.cpp"; then
  echo "Scene-tool commands must delegate to the existing Core services." >&2
  exit 1
fi

command_target="$(awk '
  /add_library\(perastage_command_scene_tools STATIC/ { capture=1 }
  capture { print }
  capture && /^\)/ { exit }
' "$root/core/CMakeLists.txt")"
command_links="$(awk '
  /target_link_libraries\(perastage_command_scene_tools PUBLIC/ { capture=1 }
  capture { print }
  capture && /^\)/ { exit }
' "$root/core/CMakeLists.txt")"

if [[ "$command_target" != *'command/command_scene_tools.cpp'* ]] ||
   rg -q '(scene_grouping|scene_node_operations|scene_object_truss_converter)\.cpp' <<<"$command_target"; then
  echo "The scene-tool target must own only its adapter implementation." >&2
  exit 1
fi

for dependency in perastage_command_core perastage_mvr_core_support \
                  perastage_scene_node_operations \
                  perastage_scene_object_truss_converter; do
  if [[ "$command_links" != *"$dependency"* ]]; then
    echo "The scene-tool target must link the production dependency: $dependency" >&2
    exit 1
  fi
done

test_block="$(awk '
  /add_executable\(command_scene_tools_test/ { capture=1 }
  capture && /add_executable\(/ && !/command_scene_tools_test/ { exit }
  capture { print }
' "$root/tests/CMakeLists.txt")"
if [[ "$test_block" != *'perastage_command_scene_tools'* ]] ||
   rg -q '(command_scene_tools|scene_grouping|scene_node_operations|scene_object_truss_converter)\.cpp' <<<"$test_block"; then
  echo "CommandSceneTools tests must link production targets without recompiling implementations." >&2
  exit 1
fi

if rg -n 'command/command_scene_tools|command_scene_tools\.h' "${domain_files[@]}"; then
  echo "Scene domain services must not depend upward on command scene tools." >&2
  exit 1
fi
