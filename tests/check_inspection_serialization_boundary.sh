#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
core_cmake="$repo_root/core/CMakeLists.txt"
tests_cmake="$repo_root/tests/CMakeLists.txt"
header="$repo_root/core/inspection/inspection_json_serializer.h"
source_file="$repo_root/core/inspection/inspection_json_serializer.cpp"
serializer_source='inspection/inspection_json_serializer.cpp'

cmake_files=("$repo_root/CMakeLists.txt" "$repo_root"/*/CMakeLists.txt)
ownership_count="$(awk -v source="$serializer_source" '
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
  echo "inspection_json_serializer.cpp must have exactly one production CMake registration." >&2
  exit 1
fi

# Emits every complete CMake statement that mentions the serialization target.
serialization_statements() {
  awk -v target='perastage_inspection_serialization' '
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

expected_core_configuration='add_library(perastage_inspection_serialization STATIC ${CMAKE_CURRENT_SOURCE_DIR}/inspection/inspection_json_serializer.cpp )
target_compile_features(perastage_inspection_serialization PUBLIC cxx_std_20)
target_include_directories(perastage_inspection_serialization PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} PRIVATE ${CMAKE_SOURCE_DIR}/third_party )
target_link_libraries(perastage_inspection_serialization PUBLIC perastage_inspection_core perastage_inspection_gdtf perastage_inspection_mvr perastage_inspection_resource )'
if [[ "$(serialization_statements "$core_cmake")" != "$expected_core_configuration" ]]; then
  echo "perastage_inspection_serialization must retain its focused production configuration." >&2
  serialization_statements "$core_cmake" >&2
  exit 1
fi

expected_test_configuration='target_link_libraries(inspection_json_serializer_test PRIVATE perastage_inspection_serialization)'
if [[ "$(serialization_statements "$tests_cmake")" != "$expected_test_configuration" ]]; then
  echo "InspectionJsonSerializer must link the production serialization target." >&2
  exit 1
fi

for cmake_file in "${cmake_files[@]}"; do
  if [[ "$cmake_file" == "$core_cmake" || "$cmake_file" == "$tests_cmake" ||
        "$cmake_file" == "$repo_root/cli/CMakeLists.txt" ]]; then
    continue
  fi
  if [[ -n "$(serialization_statements "$cmake_file")" ]]; then
    echo "Unexpected inspection serialization target consumer: $cmake_file" >&2
    exit 1
  fi
done

if rg -n '(json\.hpp|nlohmann::json)' "$header"; then
  echo "The public serializer API must not expose the JSON implementation type." >&2
  exit 1
fi

forbidden='(wx[A-Z]|MainWindow|ConfigManager|CredentialStore|ConsolePanel|Viewer[23]D|curl|tinyxml|OpenGL)'
if rg -n "$forbidden" "$header" "$source_file"; then
  echo "Inspection serialization must remain independent of UI, runtime, networking, and graphics services." >&2
  exit 1
fi

if rg -n 'inspection_json_serializer\.cpp' "$tests_cmake"; then
  echo "InspectionJsonSerializer must not compile a private serializer implementation." >&2
  exit 1
fi

echo "OK: inspection JSON serialization has one focused production owner and a clean public boundary."
