#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
files=("$root"/viewer2d/interaction/viewer2d_{navigation,selection,interaction_scope}_policy.{h,cpp})

forbidden='wxWidgets|wx/|wx(Point|MouseEvent|KeyEvent|Window|GLCanvas)|GL/|OpenGL/|glew|Viewer2DPanel|ConfigManager|TablePanel|MainWindow'
if rg -n "$forbidden" "${files[@]}"; then
  echo "Viewer2D decision policy boundary contains an adapter dependency." >&2
  exit 1
fi

echo "Viewer2D decision policy boundary check passed."
