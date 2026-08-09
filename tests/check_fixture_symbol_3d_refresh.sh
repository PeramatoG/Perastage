#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mainwindow="$root/gui/mainwindow.cpp"

if ! rg -q 'viewportPanel->RefreshAfterFixtureResourceRebind\(\)' "$mainwindow"; then
  echo "Fixture symbol publication must request 3D resource synchronization." >&2
  exit 1
fi
rg -q 'UpdateScene\(\)' "$root/viewer3d/viewer3dpanel.cpp"
echo "Fixture symbol publication refreshes 3D resource identity."
