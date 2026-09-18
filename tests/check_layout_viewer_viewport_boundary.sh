#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
files=(
  "$root/gui/layout_viewer_viewport_state.h"
  "$root/gui/layout_viewer_viewport_state.cpp"
)

forbidden='wx|GL/|OpenGL|LayoutViewerPanel|MainWindow|ConfigManager|LayoutManager|Dialog|TablePanel|Texture'
if rg -n "$forbidden" "${files[@]}"; then
  echo "Layout viewer viewport policy contains a forbidden GUI/application dependency." >&2
  exit 1
fi

echo "Layout viewer viewport boundary check passed."
