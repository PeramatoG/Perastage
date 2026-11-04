#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
service_files=("$repo_root"/mvr/mvr_source_structure*.h "$repo_root"/mvr/mvr_source_structure*.cpp
               "$repo_root"/mvr/mvr_source_recovery.{h,cpp})
if rg -n 'ConfigManager|wx[A-Z]|MvrExporter|MvrImporter|ofstream|wxZip|system\(' "${service_files[@]}"; then
  echo "MVR source structure must remain a scene/XML service without GUI, archive, or hidden session access." >&2
  exit 1
fi
rg -q 'shared_ptr<const MvrSourceStructure>' "$repo_root/models/mvrscene.h"
rg -q 'PrepareSourceStructureEdits\(result.scene\)' "$repo_root/mvr/mvr_export_preparation.cpp"
rg -q 'CaptureSourceStructure' "$repo_root/mvr/mvr_read_service.cpp"
rg -q 'ProjectSourceStructure' "$repo_root/mvr/mvrexporter.cpp"
for unit in capture projection edits merge; do
  rg -q "mvr_source_structure_${unit}.cpp" "$repo_root/mvr/CMakeLists.txt"
done
rg -q 'mvr_source_recovery.cpp' "$repo_root/mvr/CMakeLists.txt"
echo "MVR source structure boundary check passed."
