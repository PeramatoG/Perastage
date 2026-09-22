#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
header="$root/core/inspection/mvr_inspection.h"
source="$root/core/inspection/mvr_inspection.cpp"
cmake="$root/core/CMakeLists.txt"
mvr_cmake="$root/mvr/CMakeLists.txt"
tests_cmake="$root/tests/CMakeLists.txt"

fail() {
  echo "MVR inspection boundary check failed: $1" >&2
  exit 1
}

grep -Fq 'add_library(perastage_inspection_mvr STATIC' "$cmake" ||
  fail "the production inspection target is missing"
grep -Fq 'perastage_inspection_core perastage_inspection_package' "$cmake" ||
  fail "the service must compose the neutral contract and package inventory"
inspection_links="$(sed -n '/target_link_libraries(perastage_inspection_mvr/,/^)/p' "$cmake")"
printf '%s\n' "$inspection_links" | grep -Fq 'perastage_mvr_read' ||
  fail "the inspection target must link the production MVR read seam"
if printf '%s\n' "$inspection_links" | grep -Fq '${PROJECT_NAME}'; then
  fail "the inspection target must not link the application"
fi

grep -Fq 'add_library(perastage_mvr_read STATIC' "$mvr_cmake" ||
  fail "the reusable MVR read target is missing"
read_block="$(sed -n '/add_library(perastage_mvr_read STATIC/,/^)/p' "$mvr_cmake")"
if printf '%s\n' "$read_block" | grep -Eiq 'project_application|merge|export|xchange|download|gdtfnet|dialog|gui'; then
  fail "the read target contains application, mutation, network, or GUI sources"
fi

for source_name in mvr_import_package.cpp mvr_import_reference_resolver.cpp \
  mvr_import_resource_resolver.cpp mvr_read_service.cpp \
  mvr_scene_node_reader.cpp mvr_scene_node_reader_support.cpp; do
  count="$(grep -F -c "\${CMAKE_CURRENT_SOURCE_DIR}/$source_name" "$mvr_cmake" || true)"
  [ "$count" -eq 1 ] || fail "$source_name must have one production owner"
  if grep -Fq "../mvr/$source_name" "$tests_cmake"; then
    fail "tests must link the production read target instead of compiling $source_name"
  fi
done

standalone_block="$(sed -n '/add_executable(mvr_inspection_standalone_test/,/add_test(NAME MvrInspectionStandalone/p' "$tests_cmake")"
printf '%s\n' "$standalone_block" | grep -Fq 'perastage_inspection_mvr' ||
  fail "the standalone consumer must link the production inspection target"
if printf '%s\n' "$standalone_block" | grep -Eiq 'stub|\.cpp.*\.cpp|gui|viewer'; then
  fail "the standalone consumer must not compile private readers or GUI stubs"
fi

if grep -Eiq 'wx[A-Z]|wxWidgets|MainWindow|ConfigManager|viewer|download|gdtfnet|canonical' "$header"; then
  fail "the public API exposes GUI, project, network, or mutation dependencies"
fi
if grep -Eiq 'MainWindow|ConfigManager|viewer|gdtfnet|download|ImportAndRegister|ReplaceProject' "$source"; then
  fail "the service may not apply projects or invoke GUI/network workflows"
fi
grep -Fq 'ReadAcquiredMvrPackage' "$source" ||
  fail "the service must use the acquired-package read seam"

echo "MVR inspection boundary check passed"
