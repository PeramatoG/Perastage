#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
run_test_python - <<'PYTHON'
from pathlib import Path
container = Path('gui/preferencesdialog.cpp').read_text()
assert 'wxTreebook' in container and 'wxNotebook' not in container
for widget in ['new wxCheckBox', 'new wxChoice', 'new wxTextCtrl', 'new wxRadioButton']:
    assert widget not in container, 'PreferencesDialog coordinates focused pages'
assert 'wxRESIZE_BORDER' in container
assert 'page->ApplyPreferences(preferences)' in container
assert 'EVT_UI_PREFERENCES_APPLIED' in container and 'EVT_UI_UNITS_CHANGED' in container
assert 'ShowLanguageRestartNoticeIfNeeded' in container
for name in ['language', 'units', 'updates', 'rider_import', 'viewer3d',
             'selection_movement', 'gdtf', 'mvr']:
    page = Path(f'gui/preferences/{name}_preferences_page.cpp').read_text()
    assert 'LoadPreferences(' in page and 'ApplyPreferences(' in page
    assert 'SaveUserConfig(' not in page, 'Pages stage edits; container commits globally'
    assert f'preferences/{name}_preferences_page.cpp' in Path('gui/CMakeLists.txt').read_text()
assert 'wxEVT_TREEBOOK_PAGE_CHANGED' not in container, 'Navigation does not apply/reload settings'
legacy = Path('gui/preferencesdialog_persistence.cpp').read_text()
assert all(token in legacy for token in ['CaptureDirtyState', 'RestoreDirtyState', 'SaveUserConfig'])
gdtf = Path('gui/preferences/gdtf_preferences_page.cpp').read_text()
assert 'GdtfCredentialsPanel' in gdtf and 'GdtfMutationPolicyPanel' in gdtf
print('OK: Preferences page ownership, navigation and persistence boundary')
PYTHON
