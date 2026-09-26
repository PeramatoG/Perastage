#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
core_cmake="$repo_root/core/CMakeLists.txt"
cli_cmake="$repo_root/cli/CMakeLists.txt"
tests_cmake="$repo_root/tests/CMakeLists.txt"
header="$repo_root/core/inspection/inspection_report_json_serializer.h"
source_file="$repo_root/core/inspection/inspection_report_json_serializer.cpp"
report_source='inspection/inspection_report_json_serializer.cpp'

cmake_files=("$repo_root/CMakeLists.txt" "$repo_root"/*/CMakeLists.txt)
ownership_count="$(awk -v source="$report_source" '
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
  echo "inspection_report_json_serializer.cpp must have exactly one production CMake registration." >&2
  exit 1
fi

# Emits complete CMake statements that mention the exact report target token.
report_statements() {
  awk -v target='perastage_inspection_report_serialization' '
    function emit_statement() {
      normalized = statement
      gsub(/[[:space:]]+/, " ", normalized)
      sub(/^ /, "", normalized)
      sub(/ $/, "", normalized)
      target_pattern = "(^|[^A-Za-z0-9_])" target "([^A-Za-z0-9_]|$)"
      if (normalized ~ target_pattern)
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

expected_core_configuration='add_library(perastage_inspection_report_serialization STATIC ${CMAKE_CURRENT_SOURCE_DIR}/inspection/inspection_report_json_serializer.cpp )
target_compile_features(perastage_inspection_report_serialization PUBLIC cxx_std_20 )
target_include_directories(perastage_inspection_report_serialization PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} PRIVATE ${CMAKE_SOURCE_DIR}/third_party )
target_link_libraries(perastage_inspection_report_serialization PUBLIC perastage_inspection_serialization perastage_inspection_gdtf perastage_inspection_mvr perastage_inspection_resource )'
if [[ "$(report_statements "$core_cmake")" != "$expected_core_configuration" ]]; then
  echo "Report serialization must retain its reviewed Core production configuration." >&2
  report_statements "$core_cmake" >&2
  exit 1
fi

expected_cli_configuration='target_link_libraries(perastage_cli_support PUBLIC perastage_inspection_gdtf perastage_inspection_mvr perastage_inspection_resource perastage_inspection_report_serialization)'
if [[ "$(report_statements "$cli_cmake")" != "$expected_cli_configuration" ]]; then
  echo "CLI must consume the production report serialization target." >&2
  exit 1
fi

expected_test_configuration='target_link_libraries(inspection_report_json_serializer_test PRIVATE perastage_inspection_report_serialization)'
if [[ "$(report_statements "$tests_cmake")" != "$expected_test_configuration" ]]; then
  echo "InspectionReportJsonSerializer must link the production report target." >&2
  exit 1
fi

for cmake_file in "${cmake_files[@]}"; do
  if [[ "$cmake_file" == "$core_cmake" || "$cmake_file" == "$cli_cmake" ||
        "$cmake_file" == "$tests_cmake" ]]; then
    continue
  fi
  if [[ -n "$(report_statements "$cmake_file")" ]]; then
    echo "Unexpected report serialization target consumer: $cmake_file" >&2
    exit 1
  fi
done

if rg -n '(json\.hpp|nlohmann::json)' "$header"; then
  echo "The public report serializer API must not expose the JSON implementation type." >&2
  exit 1
fi

forbidden='(wx[A-Z]|MainWindow|ConfigManager|CredentialStore|ConsolePanel|Viewer[23]D|curl|tinyxml|OpenGL)'
if rg -n "$forbidden" "$header" "$source_file"; then
  echo "Report serialization must remain independent of UI, runtime, networking, and graphics services." >&2
  exit 1
fi

if rg -n 'inspection_report_json_serializer\.cpp' "$tests_cmake"; then
  echo "InspectionReportJsonSerializer must not compile a private serializer implementation." >&2
  exit 1
fi

if rg -n 'inspection_report_json_serializer\.cpp' "$repo_root/CMakeLists.txt"; then
  echo "The application target must not own report serialization sources." >&2
  exit 1
fi

echo "OK: complete inspection report serialization has one focused Core owner and reviewed consumers."
