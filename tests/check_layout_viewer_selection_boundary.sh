#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
files=(
  "$root/gui/layout_viewer_selection_state.h"
  "$root/gui/layout_viewer_selection_state.cpp"
)

forbidden='wx/|wxWidgets|OpenGL|LayoutViewerPanel|MainWindow|ConfigManager|LayoutManager|[Dd]ialog|[Tt]able[Pp]anel|[Tt]exture'
if rg -n "$forbidden" "${files[@]}"; then
  echo "Layout Viewer selection policy contains a forbidden GUI/application dependency." >&2
  exit 1
fi

echo "Layout Viewer selection boundary check passed."
