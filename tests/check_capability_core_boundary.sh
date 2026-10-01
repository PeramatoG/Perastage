#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
files=("$root"/core/capability/*.{h,cpp})
forbidden='ConfigManager|MainWindow|wx(App|Window|Panel)|viewer[23]|MutationHost|ExecutionContext|function<|Execute[[:space:]]*\(|dispatch|ipc|osc|mcp'
if rg -n -i "$forbidden" "${files[@]}"; then
  echo "Capability Core contains a forbidden execution, GUI, or transport dependency." >&2
  exit 1
fi

cmake="$root/core/CMakeLists.txt"
core_block="$(awk '/add_library\(perastage_capability_core/{active=1} /add_library\(perastage_capability_serialization/{active=0} active' "$cmake")"
serialization_block="$(awk '/add_library\(perastage_capability_serialization/{active=1} /target_compile_features\(perastage_query_core/{active=0} active' "$cmake")"
cmake_forbidden='perastage_(command_(transform|selection|scene_tools)|query_core|cli|gui|app|viewer|.*(ipc|osc|mcp|transport|mutation))'
if rg -n -i "$cmake_forbidden" <<<"$core_block"; then
  echo "perastage_capability_core links an execution, presentation, or transport target." >&2
  exit 1
fi
if ! rg -q 'target_link_libraries\(perastage_capability_core PUBLIC perastage_command_core\)' <<<"$core_block"; then
  echo "perastage_capability_core must depend only on the lightweight Command contract target." >&2
  exit 1
fi
serialization_links="$(awk '/target_link_libraries\(perastage_capability_serialization/{active=1} active{print} active && /\)/{exit}' <<<"$serialization_block")"
if [[ "$(tr '\n' ' ' <<<"$serialization_links" | tr -s ' ')" != *"PUBLIC perastage_capability_core"* ]] ||
   rg -q 'perastage_(command|query|cli|gui|app|viewer|.*(ipc|osc|mcp|transport|mutation))' <<<"$serialization_links"; then
  echo "perastage_capability_serialization has a dependency beyond Capability Core." >&2
  exit 1
fi
