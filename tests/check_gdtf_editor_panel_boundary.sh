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
  if [[ $(rg -c "new ${child}\(" "$panel_source") -ne 1 ]]; then
    echo "GdtfEditorPanel must instantiate ${child} exactly once." >&2
    exit 1
  fi
  if rg -q "new ${child}\(this\)" "$panel_source"; then
    echo "GdtfEditorPanel child panels must be parented to flat section content hosts." >&2
    exit 1
  fi
done

python3 - <<'PY_CHECK'
from pathlib import Path
source = Path('gui/gdtf/gdtf_editor_panel.cpp').read_text()
for section in ['metadataSection', 'typeIdentitySection', 'physicalPropertiesSection', 'modesSection']:
    if section not in source or f'{section}->Content()' not in source:
        raise SystemExit(f'{section} must be a flat section with a content host.')
PY_CHECK
for token in GdtfEditorPanelLayout GdtfEditorSection GdtfEditorPane GdtfEditorSectionPlacement GdtfEditorSectionConfiguration GdtfEditorPanelConfiguration GdtfEditorPanelPresentation metadataAvailable identityFields physicalFields; do
  if ! rg -q "$token" "$panel_header"; then
    echo "Missing composite configuration or presentation token: $token" >&2
    exit 1
  fi
done
for api in Configure SetPresentation SetUnavailable SetTwoPaneSplitterRatio GetTwoPaneSplitterRatio SetIdentityChangeCallback SetIdentityActionCallback SetPhysicalPropertyChangeCallback SetModeSelectionCallback GetIdentityValue GetPhysicalPropertyValue GetSelectedMode SetIdentityValue SetPhysicalPropertyValue SetPhysicalPropertyValidation SetModesPresentation SetModes SetSelectedMode SetChannelCount SetChannels ClearModeDetails SetMetadata SetMetadataUnavailable; do
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
if ! rg -q "class GdtfEditorFlatSection" "$panel_source" || ! rg -q "wxStaticLine" "$panel_source"; then
  echo "Composite sections must use flat title/line containers for visual grouping." >&2
  exit 1
fi
if rg -q "wxStaticBoxSizer" "$panel_header" "$panel_source"; then
  echo "Composite sections must not use the previous heavy static-box sections." >&2
  exit 1
fi
if ! rg -q "AddSingleColumnSections" "$panel_source" || ! rg -q "AddTwoPaneSections" "$panel_source"; then
  echo "Composite must implement deterministic single-column and two-pane layouts." >&2
  exit 1
fi
if ! rg -q "wxSplitterWindow \*twoPaneSplitter" "$panel_header" || ! rg -q "SplitVertically\(overviewPane, workspacePane" "$panel_source"; then
  echo "Two-pane GDTF layout must use a native splitter." >&2
  exit 1
fi
if ! rg -q "if \(!sectionConfiguration.visible\)" "$panel_source"; then
  echo "Hidden sections must be skipped by layout insertion." >&2
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
for header in "$fixture_header" "$truss_header"; do
  if [[ $(rg -c "GdtfEditorPanel \*gdtfEditorPanel|GdtfEditorPanel\* gdtfEditorPanel" "$header") -ne 1 ]]; then
    echo "${header} must own exactly one GdtfEditorPanel pointer." >&2
    exit 1
  fi
done
for source in "$fixture_source" "$truss_source"; do
  if [[ $(rg -c "new GdtfEditorPanel\(" "$source") -ne 1 ]]; then
    echo "${source} must instantiate exactly one GdtfEditorPanel with wx parent ownership." >&2
    exit 1
  fi
done
for child in GdtfMetadataPanel GdtfTypeIdentityPanel GdtfPhysicalPropertiesPanel GdtfModesPanel; do
  if rg -q "${child} \*|${child}\*|new ${child}\(" "$fixture_header" "$fixture_source" "$truss_header" "$truss_source"; then
    echo "Host dialogs must not own or directly instantiate ${child}." >&2
    exit 1
  fi
done
if ! rg -q "gdtfConfiguration.modes.title = \"Modes and channels\"" "$fixture_source"; then
  echo "Fixture Edit must keep the composite modes section visible with a host title." >&2
  exit 1
fi
if ! rg -q "gdtfConfiguration.modes.visible = false" "$truss_source"; then
  echo "Truss Edit must hide the composite modes section." >&2
  exit 1
fi
if rg -q "gdtf/gdtf_(metadata|type_identity|physical_properties|modes)_panel\.h" "$fixture_source" "$truss_source"; then
  echo "Host sources must not include obsolete direct child-panel headers." >&2
  exit 1
fi
if rg -q "Clear\(false\)|Reparent\(" "$panel_source"; then
  echo "Composite layout must not clear reusable windows destructively or reparent them at runtime." >&2
  exit 1
fi

if ! rg -q "DetachReusableSections" "$panel_header" "$panel_source" || \
   ! rg -q "overviewSizer->Detach\(metadataSection\)" "$panel_source" || \
   ! rg -q "workspaceSizer->Detach\(modesSection\)" "$panel_source"; then
  echo "Composite must detach reusable flat sections before reconfiguring layout." >&2
  exit 1
fi

echo "OK: GDTF editor panel boundary passed."
