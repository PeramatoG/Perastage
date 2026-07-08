#!/usr/bin/env bash
set -euo pipefail

panel_files=(
  gui/gdtf/gdtf_physical_properties_panel.h
  gui/gdtf/gdtf_physical_properties_panel.cpp
  gui/gdtf/gdtf_type_identity_panel.h
  gui/gdtf/gdtf_type_identity_panel.cpp
  gui/gdtf/gdtf_modes_panel.h
  gui/gdtf/gdtf_modes_panel.cpp
)
for file in "${panel_files[@]}"; do
  [[ -f "$file" ]] || { echo "Missing reusable panel file: $file" >&2; exit 1; }
done

if ! rg -q "class GdtfPhysicalPropertiesPanel : public wxPanel" gui/gdtf/gdtf_physical_properties_panel.h; then
  echo "Physical properties panel must derive from wxPanel." >&2
  exit 1
fi
if ! rg -q "class GdtfTypeIdentityPanel : public wxPanel" gui/gdtf/gdtf_type_identity_panel.h; then
  echo "Type identity panel must derive from wxPanel." >&2
  exit 1
fi
if ! rg -q "class GdtfModesPanel : public wxPanel" gui/gdtf/gdtf_modes_panel.h; then
  echo "Modes panel must derive from wxPanel." >&2
  exit 1
fi
if rg -q "ConfigManager|GuiConfigServices|FixtureTablePanel|TrussTablePanel|scene\.|Viewer[23]D|mvr|GdtfEditSession|GdtfApplyRequest|SetGdtfProperties|BuildTrussGdtfFromInstance|CreateOrUpdatePerastageLibraryDerivative|canonical" "${panel_files[@]}"; then
  echo "Reusable GDTF panels must not depend on project, table, viewer, mutation, or editor-session services." >&2
  exit 1
fi
if rg -q "GdtfPhysicalPropertiesPanel \*|GdtfPhysicalPropertiesPanel\*|GdtfTypeIdentityPanel \*|GdtfTypeIdentityPanel\*|GdtfModesPanel \*|GdtfModesPanel\*" gui/fixtureeditdialog.h gui/trusseditdialog.h; then
  echo "Fixture and Truss edit dialogs must access reusable child panels through GdtfEditorPanel." >&2
  exit 1
fi
if ! rg -q "GdtfEditorPanel \*gdtfEditorPanel|GdtfEditorPanel\* gdtfEditorPanel" gui/fixtureeditdialog.h gui/trusseditdialog.h; then
  echo "Fixture and Truss edit dialogs must own the reusable GdtfEditorPanel composite." >&2
  exit 1
fi
if rg -q "modeChoice|chCountCtrl|modelCtrl|channelList|crossSectionCtrl" gui/fixtureeditdialog.h gui/trusseditdialog.h; then
  echo "Host dialog headers must not retain duplicate reusable GDTF controls." >&2
  exit 1
fi
for source in gdtf_modes_panel.cpp gdtf_physical_properties_panel.cpp gdtf_type_identity_panel.cpp; do
  if ! rg -q "gdtf/$source" gui/CMakeLists.txt; then
    echo "Missing gui/CMakeLists.txt registration for $source." >&2
    exit 1
  fi
done

echo "OK: reusable GDTF panel boundaries passed."
