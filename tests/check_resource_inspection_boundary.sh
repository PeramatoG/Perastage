#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
header="$repo_root/core/inspection/resource_inspection.h"
implementation="$repo_root/core/inspection/resource_inspection.cpp"
cmake="$repo_root/core/CMakeLists.txt"

if rg -n '#include[[:space:]]+[<"](wx|GL|gui/|viewer)' "$header"; then
  echo "The public resource inspection contract must remain GUI and renderer independent." >&2
  exit 1
fi

if [[ "$(rg -l 'inspection/resource_inspection\.cpp' "$repo_root"/*/CMakeLists.txt | wc -l)" != "1" ]]; then
  echo "resource_inspection.cpp must have exactly one production owner." >&2
  exit 1
fi

rg -q 'add_library\(perastage_inspection_resource STATIC' "$cmake"
rg -q 'perastage_inspection_nested_gdtf' "$cmake"
rg -q 'ReadGdtfArchiveResource' "$implementation"
rg -q 'maxBytes' "$implementation"

echo "Resource inspection keeps neutral public types, bounded reads, and established GDTF lookup reuse."
