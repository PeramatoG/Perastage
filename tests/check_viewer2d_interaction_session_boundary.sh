#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
files=(
  "$root/viewer2d/interaction/viewer2d_interaction_session.h"
  "$root/viewer2d/interaction/viewer2d_interaction_session.cpp"
)

forbidden='wxWidgets|wx/|wx(Point|MouseEvent|Window)|GL/|OpenGL/|glew|Viewer2DPanel|Viewer3D|ConfigManager|TablePanel|MainWindow|HistoryManager|(^|[^[:alnum:]_])Refresh\('
if rg -n "$forbidden" "${files[@]}"; then
  echo "Viewer2D interaction session boundary contains an adapter dependency." >&2
  exit 1
fi

echo "Viewer2D interaction session boundary check passed."
