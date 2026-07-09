#!/usr/bin/env bash
set -euo pipefail
adapter_h="core/gdtf/editor/project_fixture_gdtf_apply_adapter.h"
adapter_cpp="core/gdtf/editor/project_fixture_gdtf_apply_adapter.cpp"
fixture_source="gui/fixtureeditdialog.cpp"
truss_source="gui/trusseditdialog.cpp"
[[ -f "$adapter_h" && -f "$adapter_cpp" ]] || { echo "Missing Project Fixture GDTF apply adapter." >&2; exit 1; }
rg -q "GdtfApplyRequest" "$adapter_h" || { echo "Adapter must consume GdtfApplyRequest." >&2; exit 1; }
rg -q "GdtfApplyResult" "$adapter_h" || { echo "Adapter result must contain GdtfApplyResult." >&2; exit 1; }
if rg -q "wx/|wxString|FixtureEditDialog|FixtureTablePanel|wxDataView|Viewer2D|Viewer3D|messagebox|wxMessageBox" "$adapter_h" "$adapter_cpp"; then
  echo "Fixture apply adapter must remain non-GUI." >&2; exit 1
fi
rg -q "ProjectFixtureGdtfApplyAdapter" "$fixture_source" "$adapter_cpp" || { echo "Fixture Edit or adapter must reference ProjectFixtureGdtfApplyAdapter." >&2; exit 1; }
if rg -q "TrussApplyAdapter|project_truss_gdtf_apply_adapter" core gui models mvr viewer2d viewer3d; then
  echo "Checkpoint 08B must not introduce a Truss apply adapter." >&2; exit 1
fi
if rg -q "bool hasRejectedSessionInput" gui/fixtureeditdialog.h gui/trusseditdialog.h; then
  echo "Rejected session input must be tracked per field, not as one boolean." >&2; exit 1
fi
rg -q "rejectedSessionInputs" gui/fixtureeditdialog.h gui/trusseditdialog.h || { echo "Per-field rejected input state is required." >&2; exit 1; }
if python3 - <<'PY'
from pathlib import Path
s=Path('gui/fixtureeditdialog.cpp').read_text()
start=s.find('void FixtureEditDialog::UpdateChannels')
end=s.find('void FixtureEditDialog::OnApply', start)
raise SystemExit(0 if 'Column::ChannelCount' in s[start:end] else 1)
PY
then
  echo "UpdateChannels must not independently dirty ChannelCount." >&2; exit 1
fi
rg -q "PathUtils::PathToUtf8" gui/fixtureeditdialog.cpp core/gdtf/editor/project_fixture_gdtf_apply_adapter.cpp || { echo "Operational path presentation must use PathToUtf8." >&2; exit 1; }
echo "OK: GDTF editor Checkpoint 08B Fixture apply adapter guard passed."
