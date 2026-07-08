#!/usr/bin/env bash
set -euo pipefail

panel_header="gui/gdtf/gdtf_editor_panel.h"
panel_source="gui/gdtf/gdtf_editor_panel.cpp"
cmake_file="gui/CMakeLists.txt"
fixture_header="gui/fixtureeditdialog.h"
fixture_source="gui/fixtureeditdialog.cpp"
truss_header="gui/trusseditdialog.h"
truss_source="gui/trusseditdialog.cpp"
child_files=(
  gui/gdtf/gdtf_metadata_panel.h
  gui/gdtf/gdtf_metadata_panel.cpp
  gui/gdtf/gdtf_type_identity_panel.h
  gui/gdtf/gdtf_type_identity_panel.cpp
  gui/gdtf/gdtf_physical_properties_panel.h
  gui/gdtf/gdtf_physical_properties_panel.cpp
  gui/gdtf/gdtf_modes_panel.h
  gui/gdtf/gdtf_modes_panel.cpp
)

for file in "$panel_header" "$panel_source" "$cmake_file" "$fixture_header" "$fixture_source" "$truss_header" "$truss_source" "${child_files[@]}"; do
  [[ -f "$file" ]] || { echo "Missing required file: $file" >&2; exit 1; }
done

if ! rg -q "class GdtfEditorPanel : public wxPanel" "$panel_header"; then
  echo "GdtfEditorPanel must derive from wxPanel." >&2
  exit 1
fi
for child in GdtfMetadataPanel GdtfTypeIdentityPanel GdtfPhysicalPropertiesPanel GdtfModesPanel; do
  if [[ $(rg -c "${child} \*" "$panel_header") -ne 1 ]]; then
    echo "GdtfEditorPanel must own exactly one ${child} pointer." >&2
    exit 1
  fi
  if ! rg -q "new ${child}\(this\)" "$panel_source"; then
    echo "GdtfEditorPanel must create ${child} exactly once with wx parent ownership." >&2
    exit 1
  fi
done

for token in GdtfEditorPanelLayout GdtfEditorSectionConfiguration GdtfEditorPanelConfiguration GdtfEditorPanelPresentation metadataAvailable identityFields physicalFields; do
  if ! rg -q "$token" "$panel_header"; then
    echo "Missing composite configuration or presentation token: $token" >&2
    exit 1
  fi
done
for api in Configure SetPresentation SetUnavailable SetIdentityChangeCallback SetIdentityActionCallback SetPhysicalPropertyChangeCallback SetModeSelectionCallback GetIdentityValue GetPhysicalPropertyValue GetSelectedMode SetIdentityValue SetPhysicalPropertyValue SetPhysicalPropertyValidation SetModesPresentation SetMetadata SetMetadataUnavailable; do
  if ! rg -q "$api" "$panel_header"; then
    echo "Missing public forwarding API: $api" >&2
    exit 1
  fi
done

if ! rg -q "GDTF metadata" "$panel_header" || ! rg -q "Type identity" "$panel_header" || \
   ! rg -q "Physical properties" "$panel_header" || ! rg -q "Modes and channels" "$panel_header"; then
  echo "Default section titles must be declared in the typed configuration." >&2
  exit 1
fi
if ! rg -q "wxStaticBoxSizer" "$panel_header" "$panel_source"; then
  echo "Composite sections must use static box sizers for visual grouping." >&2
  exit 1
fi
if ! rg -q "AddSingleColumnSections" "$panel_source" || ! rg -q "AddTwoColumnSections" "$panel_source"; then
  echo "Composite must implement deterministic single-column and two-column layouts." >&2
  exit 1
fi
if ! rg -q "AddSection\(leftColumnSizer, metadataSection" "$panel_source" || \
   ! rg -q "AddSection\(leftColumnSizer, typeIdentitySection" "$panel_source" || \
   ! rg -q "AddSection\(rightColumnSizer, physicalPropertiesSection" "$panel_source" || \
   ! rg -q "AddSection\(rightColumnSizer, modesSection" "$panel_source"; then
  echo "Two-column layout must keep the documented stable arrangement." >&2
  exit 1
fi
if ! rg -q "AddSection\(root, metadataSection" "$panel_source" || \
   ! rg -q "AddSection\(root, typeIdentitySection" "$panel_source" || \
   ! rg -q "AddSection\(root, physicalPropertiesSection" "$panel_source" || \
   ! rg -q "AddSection\(root, modesSection" "$panel_source"; then
  echo "Single-column layout must keep the documented stable order." >&2
  exit 1
fi
if ! rg -q "if \(!sectionConfiguration.visible\)" "$panel_source"; then
  echo "Hidden sections must be skipped by layout insertion." >&2
  exit 1
fi
if ! rg -q "modesSection.*1, wxEXPAND" "$panel_source"; then
  echo "Modes section must be added with growable vertical proportion inside its section." >&2
  exit 1
fi
if ! rg -q "gdtf/gdtf_editor_panel.cpp" "$cmake_file"; then
  echo "GdtfEditorPanel source must be registered in gui/CMakeLists.txt." >&2
  exit 1
fi

prohibited="ConfigManager|GuiConfigServices|FixtureTablePanel|TrussTablePanel|viewer|Mvr|mvr|LoadGdtf|GetGdtfModes|GdtfArchive|GdtfEditSession|GdtfApplyRequest|SetGdtfProperties|BuildTrussGdtfFromInstance|CreateOrUpdatePerastageLibraryDerivative|canonical|Undo|dirty|Hoist"
if rg -q "$prohibited" "$panel_header" "$panel_source"; then
  echo "GdtfEditorPanel must remain presentation-only and avoid project, load, mutation, viewer, and apply dependencies." >&2
  exit 1
fi
if rg -q "gdtf_editor_panel" "${child_files[@]}"; then
  echo "Reusable child panels must not depend on the composite panel." >&2
  exit 1
fi
if rg -q "GdtfEditorPanel" "$fixture_header" "$fixture_source" "$truss_header" "$truss_source"; then
  echo "Fixture Edit and Truss Edit must not be migrated to GdtfEditorPanel in Checkpoint 06." >&2
  exit 1
fi

if rg -q "Fit\(" "$panel_source"; then
  echo "Composite layout must not repeatedly fit parent windows." >&2
  exit 1
fi

echo "OK: GDTF editor panel composition boundary checks passed."
