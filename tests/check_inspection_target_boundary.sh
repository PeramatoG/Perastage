#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
core_cmake="$repo_root/core/CMakeLists.txt"
tests_cmake="$repo_root/tests/CMakeLists.txt"
root_cmake="$repo_root/CMakeLists.txt"
contract_source='inspection/inspection_contract.cpp'

cmake_files=("$root_cmake" "$repo_root"/*/CMakeLists.txt)
ownership_count="$(awk -v source="$contract_source" '
  {
    line = $0
    sub(/#.*/, "", line)
    while ((position = index(line, source)) != 0) {
      ++count
      line = substr(line, position + length(source))
    }
  }
  END { print count + 0 }
' "${cmake_files[@]}")"
if [[ "$ownership_count" != "1" ]]; then
  echo "inspection_contract.cpp must have exactly one production CMake registration." >&2
  exit 1
fi

# Emits every complete CMake statement that mentions the inspection target.
inspection_statements() {
  awk -v target='perastage_inspection_core' '
    function emit_statement() {
      normalized = statement
      gsub(/[[:space:]]+/, " ", normalized)
      sub(/^ /, "", normalized)
      sub(/ $/, "", normalized)
      if (index(normalized, target) != 0)
        print normalized
    }
    {
      line = $0
      sub(/#.*/, "", line)
      if (!collecting) {
        if (line !~ /^[[:space:]]*[A-Za-z_][A-Za-z0-9_]*[[:space:]]*\(/)
          next
        statement = ""
        depth = 0
        collecting = 1
      }
      statement = statement " " line
      opening = gsub(/\(/, "(", line)
      closing = gsub(/\)/, ")", line)
      depth += opening - closing
      if (depth <= 0) {
        emit_statement()
        collecting = 0
      }
    }
  ' "$1"
}

expected_core_configuration='add_library(perastage_inspection_core STATIC ${CMAKE_CURRENT_SOURCE_DIR}/inspection/inspection_contract.cpp )
target_compile_features(perastage_inspection_core PUBLIC cxx_std_20)
target_include_directories(perastage_inspection_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} )
target_link_libraries(perastage_inspection_serialization PUBLIC perastage_inspection_core )
target_link_libraries(perastage_inspection_gdtf PUBLIC perastage_inspection_core perastage_inspection_package perastage_gdtf_read )
target_link_libraries(perastage_inspection_package PUBLIC perastage_inspection_core PRIVATE perastage_archive_zip_directory )'
actual_core_configuration="$(inspection_statements "$core_cmake")"
if [[ "$actual_core_configuration" != "$expected_core_configuration" ]]; then
  echo "perastage_inspection_core must retain its exact minimal production configuration." >&2
  printf '%s\n' "$actual_core_configuration" >&2
  exit 1
fi

expected_test_configuration='target_link_libraries(inspection_contract_test PRIVATE perastage_inspection_core)'
if [[ "$(inspection_statements "$tests_cmake")" != "$expected_test_configuration" ]]; then
  echo "InspectionContract must only link the production inspection target." >&2
  exit 1
fi

for cmake_file in "${cmake_files[@]}"; do
  if [[ "$cmake_file" == "$core_cmake" || "$cmake_file" == "$tests_cmake" ]]; then
    continue
  fi
  if [[ -n "$(inspection_statements "$cmake_file")" ]]; then
    echo "perastage_inspection_core may only be configured by Core and used by its reviewed consumers." >&2
    echo "Unexpected configuration: $cmake_file" >&2
    exit 1
  fi
done

if rg -n 'inspection_contract\.cpp' "$tests_cmake"; then
  echo "InspectionContract must not compile a private copy of the implementation." >&2
  exit 1
fi

echo "OK: reusable inspection target retains one implementation and only its reviewed consumers."
