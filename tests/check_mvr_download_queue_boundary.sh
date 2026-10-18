#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
run_test_python - <<'PY'
from pathlib import Path
importer = Path('mvr/mvrimporter.cpp').read_text()
dialog = Path('gui/gdtf_download_queue_dialog.cpp').read_text()
assert 'downloadInfoDialog' not in importer
assert 'RunMvrGdtfResolution(' in importer
resolution = Path('gui/mvr_gdtf_resolution_dialog.cpp').read_text()
assert 'GdtfDownloadQueueDialog::Run(' in resolution
assert ': GdtfDownloadQueueDialog(parent, false)' in resolution
assert 'ShowModal()' not in resolution
assert dialog.count('ShowModal()') == 1
assert 'wxYield' not in dialog and 'wxEventLoop' not in dialog
source_table = Path('gui/gdtf_download_queue_table.cpp').read_text()
assert 'wxDataViewToggleRenderer' in source_table
assert 'wxYield' not in source_table and 'DownloadRevision' not in source_table
assert 'ConfigManager::Get' not in source_table
assert 'ConfigManager::Get' not in dialog
for root in ('core', 'cli'):
    for path in Path(root).rglob('*'):
        if path.suffix in ('.h', '.cpp'):
            text = path.read_text()
            assert 'gdtf_download_queue_dialog.h' not in text, path
            assert 'mvr_auto_close_gdtf_download_window' not in text, path
print('OK: GUI download queue lifecycle and headless preference boundary')
PY
