#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
cd "$(dirname "$0")/.."
run_test_python - <<'PY'
from pathlib import Path
import re
import subprocess
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
# Repository retirement and single-owner invariants belong to this existing guard.
tracked = subprocess.check_output(['git', 'ls-files'], text=True).splitlines()
assert 'help.md' not in tracked and not Path('help.md').exists()
legacy = re.compile(r'\b(?:SplitHelpMarkdown|HelpMarkdown)\b|<!-- LANG:(?:en|es) -->')
for name in tracked:
    path = Path(name)
    if path.parts[0] not in ('core', 'gui', 'app', 'cli', 'cmake', 'packaging') and name != 'CMakeLists.txt':
        continue
    if path.suffix not in ('.cpp', '.h', '.cmake', '.txt') and path.name != 'PKGBUILD':
        continue
    source = path.read_text()
    assert not legacy.search(source), name
    # Package guards may assert absence, but no copy/install/application consumer remains.
    if path.name != 'PKGBUILD': assert 'help.md' not in source, name
console = Path('gui/consolepanel.cpp').read_text()
assert 'user_manual::LoadConsoleHelp' in console and 'ResolveResourcePath("help")' in console
assert 'BuildConsoleHelpContent' not in console
assert 'ExtractConsoleSection' not in console and 'ReadUtf8File' not in console
# Commands/examples belong to Markdown, never to a second Console help catalog.
help_handler = re.search(r'void ConsolePanel::OnHelpButton.*?\n\}', console, re.S).group()
assert not re.search(r'"(?:pos |rot |f [0-9]|t [0-9])', help_handler)
manual = Path('docs/user/shortcuts-and-command-bar.md').read_text()
begin, end = '<!-- PERASTAGE_CONSOLE_HELP_BEGIN -->', '<!-- PERASTAGE_CONSOLE_HELP_END -->'
assert manual.count(begin) == manual.count(end) == 1
assert manual.index(begin) < manual.index(end)
assert manual[manual.index(begin) + len(begin):manual.index(end)].strip()
print('OK: canonical manual/Console ownership, retired legacy APIs and thin GUI wiring')
PY
