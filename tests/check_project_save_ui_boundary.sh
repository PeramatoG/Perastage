#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
cd "$(dirname "${BASH_SOURCE[0]}")/.."
run_test_python - <<'PY'
from pathlib import Path
source = Path('gui/mainwindow_io.cpp').read_text()
for name, end in [('OnSave', 'OnSaveAs'), ('OnSaveAs', 'OnImportRider')]:
    body = source.split(f'void MainWindow::{name}(')[1].split(f'void MainWindow::{end}(')[0]
    assert body.count('project_save_ui::Run(') == 1
    assert body.index('project_save_ui::Run(') < body.index('wxMessageBox(')
    assert 'wxYield' not in body and 'wxBusyInfo' not in body and 'saveOverlay' not in body
    assert 'ProjectSaveFailureKind::Unexpected' in body
    assert '_("Error"), wxOK | wxICON_ERROR, this)' in body
    if name == 'OnSaveAs':
        assert body.index('wxID_CANCEL') < body.index('project_save_ui::Run(')
helper = Path('gui/project_save_ui.cpp').read_text()
assert 'wxSTAY_ON_TOP' not in helper and 'wxYield' not in helper
assert 'wxBORDER_SIMPLE' in helper and 'overlay_->Hide()' in helper
assert helper.index('overlay_.reset();') < helper.index('disabler_.reset();')
print('Project save UI boundary check passed.')
PY
