#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
core_cmake="$repo_root/core/CMakeLists.txt"
tests_cmake="$repo_root/tests/CMakeLists.txt"
header="$repo_root/core/inspection/package_inspection.h"
source_file="$repo_root/core/inspection/package_inspection.cpp"
zip_source="$repo_root/core/archive_zip_directory.cpp"
dependencies_cmake="$repo_root/cmake/PerastageDependencies.cmake"

owner_count="$(rg -l 'inspection/package_inspection\.cpp' "$repo_root"/*/CMakeLists.txt | wc -l | tr -d ' ')"
if [[ "$owner_count" != "1" ]]; then
  echo "package_inspection.cpp must have exactly one production owner." >&2
  exit 1
fi

if ! rg -Uq 'add_library\(perastage_inspection_package STATIC[[:space:]]+\$\{CMAKE_CURRENT_SOURCE_DIR\}/inspection/package_inspection\.cpp' "$core_cmake"; then
  echo "perastage_inspection_package must be a focused static library." >&2
  exit 1
fi
if ! rg -Uq 'target_link_libraries\(perastage_inspection_package[[:space:]]+PUBLIC perastage_inspection_core[[:space:]]+PRIVATE perastage_archive_zip_directory[[:space:]]+\)' "$core_cmake"; then
  echo "Package inspection must expose Core and privately reuse ZIP metadata." >&2
  exit 1
fi
zip_owner_count="$(rg -l 'archive_zip_directory\.cpp' "$repo_root"/*/CMakeLists.txt | wc -l | tr -d ' ')"
if [[ "$zip_owner_count" != "1" ]]; then
  echo "archive_zip_directory.cpp must have exactly one production owner." >&2
  exit 1
fi
if rg -n 'wx/|wxString|inspection/' "$repo_root/core/archive_zip_directory.h" "$zip_source"; then
  echo "Raw ZIP directory metadata must remain standard-library-only." >&2
  exit 1
fi
if rg -n 'ReadLe(16|32)|06054b50|02014b50' "$source_file"; then
  echo "Package inspection must consume rather than duplicate raw ZIP mechanics." >&2
  exit 1
fi
if ! rg -Uq 'target_link_libraries\(package_inspection_test PRIVATE[[:space:]]+perastage_inspection_package' "$tests_cmake"; then
  echo "PackageInspection must link the production package target." >&2
  exit 1
fi
if rg -q 'package_inspection\.cpp' "$tests_cmake"; then
  echo "PackageInspection must not compile a private implementation copy." >&2
  exit 1
fi
if rg -n 'wx[A-Z]|MainWindow|ConfigManager|viewer|json\.hpp' "$header"; then
  echo "The public package inspection API must remain framework-neutral." >&2
  exit 1
fi
if rg -n 'wx/|wx[A-Z]|wxIMPLEMENT_APP|wxTheApp' "$source_file"; then
  echo "Package inspection implementation must remain independent of wxWidgets." >&2
  exit 1
fi
if rg -n '_wx_base_libs' "$core_cmake" "$dependencies_cmake"; then
  echo "Package inspection must not introduce wx base-only link plumbing." >&2
  exit 1
fi
if rg -n 'wx/|wx[A-Z]' "$repo_root/tests/package_inspection_test.cpp"; then
  echo "PackageInspection fixtures must remain byte-exact and wx-independent." >&2
  exit 1
fi
for consumer in "$repo_root/core/gdtf_archive_reader.cpp" "$repo_root/mvr/mvr_import_package.cpp" "$source_file"; do
  if ! rg -q 'archive_entry_path\.h' "$consumer"; then
    echo "Archive consumers must reuse the shared entry-path safety policy: $consumer" >&2
    exit 1
  fi
done

echo "OK: package inspection has one owner and an isolated base/archive dependency."
