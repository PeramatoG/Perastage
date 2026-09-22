#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
core_cmake="$root/core/CMakeLists.txt"
tests_cmake="$root/tests/CMakeLists.txt"
public_header="$root/core/inspection/gdtf_inspection.h"
dependencies_cmake="$root/cmake/PerastageDependencies.cmake"

fail() {
  echo "GDTF inspection boundary check failed: $1" >&2
  exit 1
}

for source in gdtf_archive_reader.cpp gdtf_description_reader.cpp gdtf/editor/gdtf_document.cpp; do
  count="$(grep -F -c "\${CMAKE_CURRENT_SOURCE_DIR}/$source" "$core_cmake" || true)"
  [ "$count" -eq 1 ] || fail "$source must have exactly one production owner"
  if grep -Fq "../core/$source" "$tests_cmake"; then
    fail "tests must link the production owner instead of compiling $source"
  fi
done

grep -Fq 'add_library(perastage_gdtf_read STATIC' "$core_cmake" ||
  fail "perastage_gdtf_read target is missing"
grep -Fq 'add_library(perastage_inspection_gdtf STATIC' "$core_cmake" ||
  fail "perastage_inspection_gdtf target is missing"
grep -Fq 'PUBLIC perastage_inspection_core perastage_inspection_package' "$core_cmake" ||
  fail "inspection target must reuse the neutral and package targets"
inspection_link_block="$(sed -n '/target_link_libraries(perastage_inspection_gdtf/,/^)/p' "$core_cmake")"
printf '%s\n' "$inspection_link_block" | grep -Fq 'perastage_gdtf_read' ||
  fail "inspection target must consume the shared GDTF read target"

application_link_block="$(sed -n '/target_link_libraries(${PROJECT_NAME} PRIVATE/,/^)/p' "$core_cmake")"
printf '%s\n' "$application_link_block" | grep -Fq 'perastage_gdtf_read' ||
  fail "application must consume the shared GDTF read target"
if printf '%s\n' "$application_link_block" | grep -Fq 'perastage_inspection_gdtf'; then
  fail "application must not speculatively link the GDTF inspection service"
fi

read_link_block="$(sed -n '/target_link_libraries(perastage_gdtf_read/,/^)/p' "$core_cmake")"
printf '%s\n' "$read_link_block" | grep -Fq '${_wx_base_libs}' ||
  fail "GDTF read target must use the dedicated wx base dependency"
if printf '%s\n' "$read_link_block" | grep -Fq '${_wx_libs}'; then
  fail "GDTF read target must not link the aggregate wx dependency list"
fi
grep -Fq 'set(_wx_base_libs wx::base)' "$dependencies_cmake" ||
  fail "Windows wx base dependency must use wx::base"
grep -Fq '${wxWidgets_SELECT_OPTIONS} --libs base' "$dependencies_cmake" ||
  fail "Linux and macOS wx base dependency must reuse selected wx-config options"

if grep -Eiq 'wx[A-Z]|wxWidgets|nlohmann|MainWindow|ConfigManager|viewer|GdtfEditSession' \
    "$public_header" "$root/core/gdtf_archive_reader.h" \
    "$root/core/gdtf_description_reader.h" \
    "$root/core/gdtf/editor/gdtf_document.h"; then
  fail "public inspection API exposes a GUI, JSON, viewer, or mutation type"
fi

read_block="$(sed -n '/add_library(perastage_gdtf_read STATIC/,/^)/p' "$core_cmake")"
if printf '%s\n' "$read_block" | grep -Eiq 'edit_session|mutation|canonical|download|network|viewer'; then
  fail "read target contains mutation, canonicalization, network, or viewer sources"
fi

echo "GDTF inspection boundary check passed"
