#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep

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
if rg -q "GdtfMetadataPanel\*|GdtfMetadataPanel \*" "$fixture_header" "$truss_header"; then
  echo "Fixture and Truss edit dialogs must not own GdtfMetadataPanel directly." >&2
  exit 1
fi
if ! rg -q "GdtfEditorPanel \*gdtfEditorPanel|GdtfEditorPanel\* gdtfEditorPanel" "$fixture_header" "$truss_header"; then
  echo "Fixture and Truss edit dialogs must receive metadata through GdtfEditorPanel." >&2
  exit 1
fi
if rg -q "metadataDescriptionCtrl|metadataValueLabels|metadataGrid|metadataLabels" "$fixture_header" "$fixture_source" "$truss_header" "$truss_source"; then
  echo "Fixture and Truss edit dialogs must not keep duplicated metadata controls or label grids." >&2
  exit 1
fi
if rg -q "new GdtfMetadataPanel\(this\)" "$fixture_source" "$truss_source"; then
  echo "Host dialogs must not construct GdtfMetadataPanel directly." >&2
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
if ! rg -q "wxTE_MULTILINE" "$panel_source"; then
  echo "Description control must remain multiline." >&2
  exit 1
fi
if ! rg -q "SetDescriptionEditable" "$panel_header" "$panel_source" || \
   ! rg -q "SetDescriptionChangeCallback" "$panel_header" "$panel_source" || \
   ! rg -q "if (updating || !descriptionChangeCallback || !descriptionCtrl)" "$panel_source"; then
  echo "Description editability must be controlled through the public panel API without callbacks during programmatic updates." >&2
  exit 1
fi
if ! rg -q "metadataPanel->SetDescriptionEditable" gui/gdtf/gdtf_editor_panel.cpp || \
   ! rg -q "metadataPanel->SetDescriptionChangeCallback" gui/gdtf/gdtf_editor_panel.cpp; then
  echo "GdtfEditorPanel must be the host-facing metadata description editing boundary." >&2
  exit 1
fi
if ! rg -q "SetEditable\(editable\)" "$panel_source"; then
  echo "Read-only mode must remain selectable through SetDescriptionEditable(false)." >&2
  exit 1
fi

if ! rg -q "std::array<wxString, 8> currentValues" "$panel_header" || \
   ! rg -q "currentValues = values" "$panel_source" || \
   ! rg -q "label->SetLabel\(currentValues\[i\]\)" "$panel_source"; then
  echo "GdtfMetadataPanel must rewrap from stored unwrapped presentation values." >&2
  exit 1
fi
if ! rg -q "kMinimumValueWrapWidth = 120" "$panel_source" || \
   ! rg -q "kInitialValueWrapWidth = 300" "$panel_source" || \
   ! rg -q "std::max\(kMinimumValueWrapWidth, valueWidth\)" "$panel_source"; then
  echo "GdtfMetadataPanel must use actual value-column width with a small safe lower bound." >&2
  exit 1
fi
if ! rg -q "lastAppliedWrapWidth" "$panel_header" "$panel_source" || \
   ! rg -q "if \(!force && width == lastAppliedWrapWidth\)" "$panel_source"; then
  echo "GdtfMetadataPanel must avoid repeated same-width resize rewrites." >&2
  exit 1
fi
if rg -q "std::max\(kMinimumWrapWidth" "$panel_source"; then
  echo "GdtfMetadataPanel must not force a 300-pixel width for narrow value columns." >&2
  exit 1
fi
if ! run_test_python - <<'PY'
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
