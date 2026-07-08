#!/usr/bin/env bash
set -euo pipefail

panel_header="gui/gdtf/gdtf_metadata_panel.h"
panel_source="gui/gdtf/gdtf_metadata_panel.cpp"
fixture_header="gui/fixtureeditdialog.h"
fixture_source="gui/fixtureeditdialog.cpp"
truss_header="gui/trusseditdialog.h"
truss_source="gui/trusseditdialog.cpp"
cmake_file="gui/CMakeLists.txt"
metadata_core="core/gdtf_metadata_summary.cpp"

for file in "$panel_header" "$panel_source" "$fixture_header" "$fixture_source" "$truss_header" "$truss_source" "$cmake_file" "$metadata_core"; do
  if [[ ! -f "$file" ]]; then
    echo "Missing required file: $file" >&2
    exit 1
  fi
done

if ! rg -q "class GdtfMetadataPanel : public wxPanel" "$panel_header"; then
  echo "GdtfMetadataPanel must be a wxPanel-derived class." >&2
  exit 1
fi
if ! rg -q "void SetMetadata\(const GdtfMetadataSummary &summary\)" "$panel_header" || \
   ! rg -q "void SetUnavailable\(\)" "$panel_header"; then
  echo "GdtfMetadataPanel must expose the presentation-only SetMetadata/SetUnavailable API." >&2
  exit 1
fi
if ! rg -q "GdtfMetadataPanel\*? metadataPanel|GdtfMetadataPanel \*metadataPanel" "$fixture_header" || \
   ! rg -q "GdtfMetadataPanel \*metadataPanel" "$truss_header"; then
  echo "Fixture and Truss edit dialogs must own a GdtfMetadataPanel pointer." >&2
  exit 1
fi
if rg -q "metadataDescriptionCtrl|metadataValueLabels|metadataGrid|metadataLabels" "$fixture_header" "$fixture_source" "$truss_header" "$truss_source"; then
  echo "Fixture and Truss edit dialogs must not keep duplicated metadata controls or label grids." >&2
  exit 1
fi
if ! rg -q "new GdtfMetadataPanel\(this\)" "$fixture_source" || \
   ! rg -q "new GdtfMetadataPanel\(this\)" "$truss_source"; then
  echo "Both edit dialogs must construct GdtfMetadataPanel." >&2
  exit 1
fi
if [[ $(rg -c "UpdateMetadataSummary\(\);" "$fixture_source") -ne 2 ]]; then
  echo "Fixture Edit must retain its two metadata refresh points." >&2
  exit 1
fi
if [[ $(rg -c "UpdateMetadataSummary\(\);" "$truss_source") -ne 2 ]]; then
  echo "Truss Edit must retain its two metadata refresh points." >&2
  exit 1
fi
if [[ $(rg -c "LoadGdtfMetadataSummary\(" "$fixture_source") -ne 1 || \
      $(rg -c "LoadGdtfMetadataSummary\(" "$truss_source") -ne 1 ]]; then
  echo "Each host dialog must remain responsible for one metadata load call." >&2
  exit 1
fi
if ! rg -q "LoadGdtfMetadataSummary\(" "$metadata_core"; then
  echo "LoadGdtfMetadataSummary must remain implemented in core/." >&2
  exit 1
fi
if rg -q "configmanager|guiconfigservices|FixtureTablePanel|TrussTablePanel|viewer|mvr|GdtfEditSession|GdtfApplyRequest|gdtf_mutation|canonical" "$panel_header" "$panel_source"; then
  echo "GdtfMetadataPanel must not depend on project, table, viewer, mutation, or editor-session services." >&2
  exit 1
fi
if ! rg -q "wxTE_MULTILINE \| wxTE_READONLY" "$panel_source"; then
  echo "Description control must remain multiline and read-only." >&2
  exit 1
fi
if ! python3 - <<'PY'
from pathlib import Path
source = Path('gui/gdtf/gdtf_metadata_panel.cpp').read_text()
labels = ['Manufacturer', 'Description', 'Creation date', 'UserID', 'ModifiedBy', 'Revision', 'Last modified', 'Version']
positions = [source.find(f'"{label}"') for label in labels]
raise SystemExit(0 if all(p >= 0 for p in positions) and positions == sorted(positions) else 1)
PY
then
  echo "GdtfMetadataPanel must define the required metadata fields in order." >&2
  exit 1
fi
if ! rg -q "gdtf/gdtf_metadata_panel.cpp" "$cmake_file"; then
  echo "GdtfMetadataPanel source must be registered in gui/CMakeLists.txt." >&2
  exit 1
fi

echo "OK: GDTF metadata panel boundary and dialog migration checks passed."
