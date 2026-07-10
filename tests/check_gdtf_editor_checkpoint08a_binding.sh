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
if ! rg -q "ResolveFixtureGdtfDeterministic" "$fixture_source"; then
  echo "Fixture Edit must use deterministic fixture GDTF resolution." >&2
  exit 1
fi
if rg -n "LoadGdtfDocument\\([^\\n]*fixture\\.gdtfSpec|LoadGdtfDocument\\([^\\n]*it->second\\.gdtfSpec" "$fixture_source"; then
  echo "Fixture Edit must not pass raw fixture.gdtfSpec to LoadGdtfDocument." >&2
  exit 1
fi
if ! rg -q "GetActiveResolvedGdtfPath" "$fixture_header" "$fixture_source"; then
  echo "Fixture Edit must expose one active resolved-path helper." >&2
  exit 1
fi
python3 - <<'PY_CHECK'
from pathlib import Path
source = Path('gui/fixtureeditdialog.cpp').read_text()
for name in ['UpdateChannels', 'UpdateVisualizers', 'UpdateMetadataSummary', 'ApplyChanges']:
    start = source.find(f'FixtureEditDialog::{name}')
    if start < 0:
        raise SystemExit(f'Missing FixtureEditDialog::{name}')
    next_fn = source.find('\n// ', start + 1)
    body = source[start: next_fn if next_fn > start else len(source)]
    if 'GetActiveResolvedGdtfPath()' not in body:
        raise SystemExit(f'{name} must use GetActiveResolvedGdtfPath() for GDTF file I/O.')
    if 'GetIdentityValue(GdtfTypeIdentityField::SourceFileReference)' in body:
        raise SystemExit(f'{name} must not use the panel source field as the authoritative I/O path.')
PY_CHECK
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
if ! rg -q "RebindContextPreservingValues" "$fixture_source" || \
   ! rg -q "LoadGdtfDocument\\(input.resolvedGdtfPath\\)" "$fixture_source"; then
  echo "Fixture Browse/rebinding must update context path and document through the session." >&2
  exit 1
fi
if rg -q "GdtfEditSession|GdtfApplyRequest|BuildProjectFixtureGdtf|BuildProjectTrussGdtf" "$panel_header" "$panel_source"; then
  echo "GdtfEditorPanel must remain presentation-only and session-free." >&2
  exit 1
fi
if ! rg -q "ProjectTrussGdtfApplyAdapter" "$truss_source"; then
  echo "Checkpoint 08C requires Truss host integration through the apply adapter." >&2
  exit 1
fi
for token in EnsureGdtfForEditedTruss BuildTrussGdtfFromInstance; do
  if rg -q "$token" "$truss_source"; then
    echo "Truss host path must not use legacy generation after Checkpoint 08C: $token" >&2
    exit 1
  fi
done

echo "OK: GDTF editor Checkpoint 08A host binding checks passed."
