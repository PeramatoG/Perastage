#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
run_test_python - <<'PY'
from pathlib import Path
service = Path('core/cache_service.cpp').read_text()
assert 'wx' not in service and 'gui/' not in service
page = Path('gui/preferences/cache_preferences_page.cpp').read_text()
for forbidden in ['directory_iterator', 'remove_all', 'file_size', 'recursive_directory_iterator']:
    assert forbidden not in page
assert 'CacheService::Clear()' in page
assert 'wxNO_DEFAULT' in page
assert 'void CachePreferencesPage::ApplyPreferences(IGuiPreferencesService &) {}' in page
assert 'CachePreferencesPage(book)' in Path('gui/preferencesdialog.cpp').read_text()
assert 'sourcePath + ".cache"' not in Path('viewer3d/resources/mesh_processing.cpp').read_text()
assert 'CleanupLegacyViewerCaches' in Path('app/perastage_app.cpp').read_text()
print('OK: persistent cache service and Preferences boundary')
PY
