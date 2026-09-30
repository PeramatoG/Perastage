#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
files=("$root/core/query/query_contract.h" "$root/core/query/scene_query.h" "$root/core/query/scene_query.cpp" "$root/core/scene_object_identity.h" "$root/core/fixture_patch_address.h")
if rg -ni 'configmanager|mainwindow|tablepanel|gui/|viewer[23]d|wx(app|window|frame|panel)|command/' "${files[@]}"; then
  echo "Query Core must remain independent of GUI, application, viewer, and Command layers." >&2
  exit 1
fi
block="$(sed -n '/add_library(perastage_query_core STATIC/,/^)/p;/target_.*(perastage_query_core/,/^)/p' "$root/core/CMakeLists.txt")"
if printf '%s' "$block" | rg -q 'perastage_command|perastage_gui|viewer'; then
  echo "Query Core has an upward CMake dependency." >&2
  exit 1
fi
for consumer in "$root/core/query/scene_query.cpp" \
                "$root/core/autopatcher.cpp" \
                "$root/mvr/mvr_merge_analyzer.cpp"; do
  if ! rg -q 'fixture_patch_address\.h' "$consumer"; then
    echo "Patch consumers must use the shared Core address parser: $consumer" >&2
    exit 1
  fi
done
test_block="$(sed -n '/add_executable(scene_query_test/,/set_perastage_test_labels(SceneQuery/p' "$root/tests/CMakeLists.txt")"
if rg -q 'scene_query\.cpp' <<<"$test_block" ||
   ! rg -q 'perastage_query_core' <<<"$test_block"; then
  echo "SceneQuery must link the production Query Core target." >&2
  exit 1
fi
echo "OK: Query Core remains read-only and GUI-independent."
