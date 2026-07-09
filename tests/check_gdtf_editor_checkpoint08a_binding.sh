#!/usr/bin/env bash
set -euo pipefail

fixture_header="gui/fixtureeditdialog.h"
fixture_source="gui/fixtureeditdialog.cpp"
truss_header="gui/trusseditdialog.h"
truss_source="gui/trusseditdialog.cpp"
panel_header="gui/gdtf/gdtf_editor_panel.h"
panel_source="gui/gdtf/gdtf_editor_panel.cpp"

for file in "$fixture_header" "$fixture_source" "$truss_header" "$truss_source" "$panel_header" "$panel_source"; do
  [[ -f "$file" ]] || { echo "Missing required file: $file" >&2; exit 1; }
done

if [[ $(rg -c "std::unique_ptr<gdtf::GdtfEditSession> gdtfEditSession" "$fixture_header") -ne 1 ]]; then
  echo "Fixture Edit must own exactly one host GdtfEditSession." >&2
  exit 1
fi
if [[ $(rg -c "std::unique_ptr<gdtf::GdtfEditSession> gdtfEditSession" "$truss_header") -ne 1 ]]; then
  echo "Truss Edit must own exactly one host GdtfEditSession." >&2
  exit 1
fi
if ! rg -q "BuildProjectFixtureGdtfEditSession" "$fixture_source" || \
   ! rg -q "BuildProjectTrussGdtfEditSession" "$truss_source"; then
  echo "Hosts must construct sessions through project context/session builders." >&2
  exit 1
fi
if ! rg -q "SetSessionValue" "$fixture_source" || ! rg -q "SetValue" "$fixture_source" || \
   ! rg -q "SetSessionValue" "$truss_source" || ! rg -q "SetValue" "$truss_source"; then
  echo "Supported callbacks must route edits through GdtfEditSession::SetValue." >&2
  exit 1
fi
if ! rg -q "IsFieldDirty" "$fixture_source" || ! rg -q "IsFieldDirty" "$truss_source"; then
  echo "Session dirty state must drive legacy GDTF field flags." >&2
  exit 1
fi
if ! rg -q "ValidateSessionBeforeApply" "$fixture_source" || \
   ! rg -q "ValidateSessionBeforeApply" "$truss_source"; then
  echo "Validation must occur before legacy Apply mutation." >&2
  exit 1
fi
if rg -q "GdtfEditSession|GdtfApplyRequest|BuildProjectFixtureGdtf|BuildProjectTrussGdtf" "$panel_header" "$panel_source"; then
  echo "GdtfEditorPanel must remain presentation-only and session-free." >&2
  exit 1
fi
if rg -q "FixtureApplyAdapter|TrussApplyAdapter|GdtfApplyResult" "$fixture_source" "$truss_source" gui/gdtf; then
  echo "Checkpoint 08A must not introduce Fixture/Truss Apply adapters." >&2
  exit 1
fi
for token in SetGdtfProperties CreateOrUpdatePerastageLibraryDerivative ApplySharedPhysicalPropertyEdit ApplyModeForGdtf EnsureGdtfForEditedTruss BuildTrussGdtfFromInstance; do
  if ! rg -q "$token" "$fixture_source" "$truss_source"; then
    echo "Existing write function must remain in host paths: $token" >&2
    exit 1
  fi
done

echo "OK: GDTF editor Checkpoint 08A host binding checks passed."
