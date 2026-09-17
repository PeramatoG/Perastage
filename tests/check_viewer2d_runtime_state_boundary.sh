#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
files=(
  "$root/viewer2d/interaction/viewer2d_runtime_state.h"
  "$root/viewer2d/interaction/viewer2d_runtime_state.cpp"
)

forbidden='wxWidgets|wx/|wx(Point|Window)|GL/|OpenGL/|glew|Viewer2DPanel|Viewer3DPanel|MainWindow|ConfigManager|FixtureTablePanel|TrussTablePanel|HoistTablePanel|SceneObjectTablePanel'
if rg -n "$forbidden" "${files[@]}"; then
  echo "Viewer2D runtime state boundary contains an adapter dependency." >&2
  exit 1
fi

echo "Viewer2D runtime state boundary check passed."
