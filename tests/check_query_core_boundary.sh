#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
files=("$root/core/query/query_contract.h" "$root/core/query/scene_query.h" "$root/core/query/scene_query.cpp" "$root/core/scene_object_identity.h")
if rg -n 'ConfigManager|MainWindow|FixtureTablePanel|viewer[23]d|wx[A-Z]|command/' "${files[@]}"; then
  echo "Query Core must remain independent of GUI, application, viewer, and Command layers." >&2
  exit 1
fi
block="$(sed -n '/add_library(perastage_query_core STATIC/,/^)/p;/target_.*(perastage_query_core/,/^)/p' "$root/core/CMakeLists.txt")"
if printf '%s' "$block" | rg -q 'perastage_command|perastage_gui|viewer'; then
  echo "Query Core has an upward CMake dependency." >&2
  exit 1
fi
echo "OK: Query Core remains read-only and GUI-independent."
