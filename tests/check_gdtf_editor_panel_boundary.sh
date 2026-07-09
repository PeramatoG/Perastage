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
    echo "GdtfEditorPanel child panels must be parented to their section static boxes, not the composite panel." >&2
    exit 1
  fi
done

python3 - <<'PY_CHECK'
from pathlib import Path
source = Path('gui/gdtf/gdtf_editor_panel.cpp').read_text()
checks = [
    ('metadataSection', 'metadataPanel', 'GdtfMetadataPanel', 'metadataSection->GetStaticBox()'),
    ('typeIdentitySection', 'typeIdentityPanel', 'GdtfTypeIdentityPanel', 'typeIdentitySection->GetStaticBox()'),
    ('physicalPropertiesSection', 'physicalPropertiesPanel', 'GdtfPhysicalPropertiesPanel', 'physicalPropertiesSection->GetStaticBox()'),
    ('modesSection', 'modesPanel', 'GdtfModesPanel', 'modesSection->GetStaticBox()'),
]
for section, member, child, parent in checks:
    section_token = f'{section} = new wxStaticBoxSizer'
    section_index = source.find(section_token)
    child_index = source.find(f'{member} =', section_index)
    new_index = source.find(f'new {child}', child_index)
    parent_index = source.find(parent, new_index)
    if (section_index < 0 or child_index < 0 or new_index < 0 or
            parent_index < 0 or section_index > child_index):
        raise SystemExit(
            f'{child} must be created after {section} and parented with {parent}.')
PY_CHECK

for token in GdtfEditorPanelLayout GdtfEditorSectionConfiguration GdtfEditorPanelConfiguration GdtfEditorPanelPresentation metadataAvailable identityFields physicalFields; do
  if ! rg -q "$token" "$panel_header"; then
    echo "Missing composite configuration or presentation token: $token" >&2
    exit 1
  fi
done
for api in Configure SetPresentation SetUnavailable SetIdentityChangeCallback SetIdentityActionCallback SetPhysicalPropertyChangeCallback SetModeSelectionCallback GetIdentityValue GetPhysicalPropertyValue GetSelectedMode SetIdentityValue SetPhysicalPropertyValue SetPhysicalPropertyValidation SetModesPresentation SetModes SetSelectedMode SetChannelCount SetChannels ClearModeDetails SetMetadata SetMetadataUnavailable; do
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
if ! rg -q "new GdtfEditorPanel\(gdtfGeneralSizer->GetStaticBox\(\)\)" "$fixture_source"; then
  echo "Fixture Edit must parent its composite to the surrounding static box." >&2
  exit 1
fi
if ! rg -q "new GdtfEditorPanel\(gdtfSizer->GetStaticBox\(\)\)" "$truss_source"; then
  echo "Truss Edit must parent its composite to the surrounding static box." >&2
  exit 1
fi
if ! rg -q "new wxStaticText\(gdtfGeneralSizer->GetStaticBox\(\)" "$fixture_source"; then
  echo "Fixture Edit GDTF explanatory text must be parented to the surrounding static box." >&2
  exit 1
fi
if ! rg -q "new wxStaticText\(gdtfSizer->GetStaticBox\(\)" "$truss_source"; then
  echo "Truss Edit GDTF explanatory text must be parented to the surrounding static box." >&2
  exit 1
fi
for token in \
  "wxWindow \*fixtureSpecificParent = fixtureSpecificSizer->GetStaticBox\(\)" \
  "new wxChoice\(fixtureSpecificParent" \
  "new wxColourPickerCtrl\(fixtureSpecificParent" \
  "new wxTextCtrl\(fixtureSpecificParent" \
  "new wxStaticText\(fixtureSpecificParent" \
  "wxWindow \*symbolParent = symbolSizer->GetStaticBox\(\)" \
  "new wxStaticText\(symbolParent" \
  "new wxPanel\(symbolParent" \
  "new wxStaticBitmap\(imageSizer->GetStaticBox\(\)"; do
  if ! rg -q "$token" "$fixture_source"; then
    echo "Fixture Edit static-box contents must use static-box parents: $token" >&2
    exit 1
  fi
done
for token in \
  "wxWindow \*mvrParent = mvrSizer->GetStaticBox\(\)" \
  "new wxTextCtrl\(mvrParent" \
  "new wxStaticText\(mvrParent" \
  "new FixturePreviewPanel\(previewSizer->GetStaticBox\(\)"; do
  if ! rg -q "$token" "$truss_source"; then
    echo "Truss Edit static-box contents must use static-box parents: $token" >&2
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

if rg -q "Fit\(" "$panel_source"; then
  echo "Composite layout must not repeatedly fit parent windows." >&2
  exit 1
fi

echo "OK: GDTF editor panel Checkpoint 07 composition boundary checks passed."
