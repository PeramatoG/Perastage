#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
cd "$(dirname "$0")/.."
run_test_python - <<'PY'
from pathlib import Path
import re
core = list(Path('core/user_manual').glob('*'))
for path in core:
    text = path.read_text()
    assert not re.search(r'#include\s*[<"](?:wx/|gui/)', text), path
text = Path('gui/mainwindow_menu.cpp').read_text()
match = re.search(r'void MainWindow::OnShowHelp\([^\n]*\)\s*\{(.*?)\n\}', text, re.S)
assert match, 'Help entry handler must exist'
handler = match.group(1)
assert 'help.md' not in handler and 'SplitHelpMarkdown' not in handler
assert 'UserManualDialog' in handler and 'kUiLanguageConfigKey' in handler
assert 'ResolveResourcePath("help")' in handler
online = re.search(r'void MainWindow::OnOpenOnlineDocumentation\([^\n]*\)\s*\{(.*?)\n\}', text, re.S)
assert online, 'Online Documentation menu handler must exist'
assert 'wxLaunchDefaultBrowser' in online.group(1)
assert 'user_manual::kOnlineDocumentation' in online.group(1)
assert 'https://' not in online.group(1), 'Online Documentation must have one destination owner'
assert 'ManualResolver' in Path('gui/user_manual_dialog.h').read_text()
assert 'wxEVT_HTML_LINK_CLICKED' in Path('gui/user_manual_dialog.cpp').read_text()
print('OK: manual Core boundary and thin Help/menu wiring')
PY
