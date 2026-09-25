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
grep -Fq 'add_library(perastage_mvr_import_application_read STATIC' "$mvr_cmake" ||
  fail "application read adapters must have a separate production owner"
read_block="$(sed -n '/add_library(perastage_mvr_read STATIC/,/^)/p' "$mvr_cmake")"
if printf '%s\n' "$read_block" | grep -Eiq 'project_application|merge|export|xchange|download|gdtfnet|dialog|gui'; then
  fail "the read target contains application, mutation, network, or GUI sources"
fi
if printf '%s\n' "$read_block" | grep -Fq 'mvr_import_application_read_services'; then
  fail "the reusable read target must not own application enrichment adapters"
fi
if printf '%s\n' "$read_block" | grep -Eq '(CMAKE_SOURCE_DIR\}/|\.\./)(core|models)/.*\.cpp'; then
  fail "the reusable read target must not directly own Core or Model sources"
fi
for source_name in fixture_visual_color.cpp gdtf_fixture_category.cpp \
  scene_grouping.cpp utf8_utils.cpp; do
  grep -Fq "\${CMAKE_CURRENT_SOURCE_DIR}/$source_name" "$root/core/CMakeLists.txt" ||
    fail "$source_name must be registered by Core"
  count="$(grep -F -h "\${CMAKE_CURRENT_SOURCE_DIR}/$source_name" \
    "$root"/{app,core,gui,models,mvr,viewer2d,viewer3d,viewer_common}/CMakeLists.txt | wc -l)"
  [ "$count" -eq 1 ] || fail "$source_name must have one production owner"
done
grep -Fq '${CMAKE_CURRENT_SOURCE_DIR}/mvrscene.cpp' "$root/models/CMakeLists.txt" ||
  fail "mvrscene.cpp must be registered by Models"
count="$(grep -F -h '${CMAKE_CURRENT_SOURCE_DIR}/mvrscene.cpp' \
  "$root"/{app,core,gui,models,mvr,viewer2d,viewer3d,viewer_common}/CMakeLists.txt | wc -l)"
[ "$count" -eq 1 ] || fail "mvrscene.cpp must have one production owner"
grep -Fq 'perastage_runtime_storage' "$mvr_cmake" ||
  fail "MVR acquisition must reuse the production runtime-storage target"

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
if grep -Fq 'gdtfdictionary' "$root/mvr/mvr_scene_node_reader.h"; then
  fail "the reusable scene-reader contract must not expose the application dictionary"
fi
if grep -Eiq 'MainWindow|ConfigManager|viewer|gdtfnet|download|ImportAndRegister|ReplaceProject' "$source"; then
  fail "the service may not apply projects or invoke GUI/network workflows"
fi
grep -Fq 'ReadAcquiredMvrPackage' "$source" ||
  fail "the service must use the acquired-package read seam"

importer="$root/mvr/mvrimporter.cpp"
if grep -Eq 'FirstChildElement\("(GeneralSceneDescription|UserData|Layers|Symdef)' "$importer"; then
  fail "MvrImporter must not own GeneralSceneDescription XML traversal"
fi
if grep -Eq 'ReadMvrSceneNodes|ParseSceneXml' "$importer"; then
  fail "MvrImporter must not own a second scene-parser orchestration path"
fi
grep -Fq 'mvr::ReadAcquiredMvrPackage(' "$importer" ||
  fail "every importer mode must delegate to the shared parser"
if grep -Eq 'applyDictionary[^\n]*\?.*ReadAcquiredMvrPackage|ReadAcquiredMvrPackage[^\n]*applyDictionary' "$importer"; then
  fail "dictionary policy must not select the XML parser"
fi

echo "MVR inspection boundary check passed"
