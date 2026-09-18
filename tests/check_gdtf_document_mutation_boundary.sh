#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
core_files=(
  "$repo_root/core/gdtf_document_mutation.h"
  "$repo_root/core/gdtf_document_mutation.cpp"
)

forbidden_dependency='#[[:space:]]*include[[:space:]]*[<"]([^">]*/)?(viewer3d|gui)/|Viewer3DPanel|Viewer3DController|MainWindow|ConsolePanel|ConfigManager'
if rg -n "$forbidden_dependency" "${core_files[@]}"; then
  echo "Core GDTF document mutation must remain independent of Viewer3D and GUI services." >&2
  exit 1
fi

implementation_symbols='WriteDirectoryArchive|ReplaceArchiveAtomically|ValidateFinitePhysicalPropertyInputs|BeforeAtomicReplace|BeforeOpenTemporaryArchive'
if rg -n "$implementation_symbols" "$repo_root/viewer3d/gdtfloader.cpp"; then
  echo "Detailed GDTF mutation/publication logic must be owned by Core." >&2
  exit 1
fi

echo "GDTF document mutation boundary check passed."
