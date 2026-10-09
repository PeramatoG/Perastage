#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mainwindow="$root/gui/mainwindow.cpp"

if ! rg -q 'viewportPanel->RefreshAfterFixtureResourceRebind\(\)' "$mainwindow"; then
  echo "Fixture symbol publication must request 3D resource synchronization." >&2
  exit 1
fi
rg -q 'UpdateScene\(\)' "$root/viewer3d/viewer3dpanel.cpp"
python3 - "$root" <<'PY'
from pathlib import Path
import re
import sys

root = Path(sys.argv[1])
menu = (root / "gui/mainwindow_menu.cpp").read_text(encoding="utf-8")
for action in ("OnUndo", "OnRedo"):
    body = re.search(r"void MainWindow::" + action + r"\([^\n]*\).*?(?=\nvoid MainWindow::|\Z)",
                     menu, re.S)
    if body is None or "NotifySceneVisualContentChanged();" not in body.group(0):
        raise SystemExit(action + " must invalidate restored project symbol layout captures.")
hash_source = (root / "gui/layoutviewerpanel_render_invalidation.cpp").read_text(encoding="utf-8")
fixture_hash = re.search(r"size_t HashFixtureValue\(.*?\n\}", hash_source, re.S)
if fixture_hash is None or "BuildProjectFixtureSymbolSource(fixture)" not in fixture_hash.group(0):
    raise SystemExit("Layout scene fingerprints must include project symbol content identity.")
PY
echo "Fixture symbol publication refreshes 3D resource identity."
