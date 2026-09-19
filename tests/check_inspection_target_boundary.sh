#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
core_cmake="$repo_root/core/CMakeLists.txt"
tests_cmake="$repo_root/tests/CMakeLists.txt"
root_cmake="$repo_root/CMakeLists.txt"
contract_source='inspection/inspection_contract.cpp'

target_block="$(sed -n '/^add_library(perastage_inspection_core STATIC/,/^)/p' "$core_cmake")"
expected_source_line='    ${CMAKE_CURRENT_SOURCE_DIR}/'"$contract_source"
if ! printf '%s\n' "$target_block" | rg -F -q "$expected_source_line"; then
  echo "perastage_inspection_core must explicitly own $contract_source." >&2
  exit 1
fi

ownership_count="$(rg -l 'inspection/inspection_contract\.cpp' \
  "$root_cmake" "$repo_root"/*/CMakeLists.txt | wc -l | tr -d ' ')"
if [[ "$ownership_count" != "1" ]]; then
  echo "inspection_contract.cpp must have exactly one CMake source owner." >&2
  exit 1
fi

if ! rg -q '^target_link_libraries\(\$\{PROJECT_NAME\} PRIVATE perastage_inspection_core\)$' "$core_cmake"; then
  echo "The Perastage application must link perastage_inspection_core." >&2
  exit 1
fi

if ! rg -q '^target_link_libraries\(inspection_contract_test PRIVATE perastage_inspection_core\)$' "$tests_cmake"; then
  echo "InspectionContract must link the production inspection target." >&2
  exit 1
fi

if rg -n 'inspection_contract\.cpp' "$tests_cmake"; then
  echo "InspectionContract must not compile a private copy of the implementation." >&2
  exit 1
fi

inspection_configuration="$(sed -n \
  '/^add_library(perastage_inspection_core STATIC/,/^target_link_libraries(${PROJECT_NAME} PRIVATE perastage_inspection_core)$/p' \
  "$core_cmake")"
if printf '%s\n' "$inspection_configuration" | rg -n '(gui/|app/|viewer2d/|viewer3d/|wxWidgets|_wx_|tinyxml2|CURL|OpenGL)'; then
  echo "The reusable inspection target acquired a forbidden application or framework dependency." >&2
  exit 1
fi

echo "OK: reusable inspection target has one explicit implementation and two independent consumers."
