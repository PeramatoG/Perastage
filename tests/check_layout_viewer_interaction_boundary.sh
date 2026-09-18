#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
files=(
  "$root/gui/layout_viewer_interaction_session.h"
  "$root/gui/layout_viewer_interaction_session.cpp"
)

forbidden='wx[A-Z]|GL/|OpenGL|LayoutViewerPanel|ConfigManager|MainWindow|Dialog|TablePanel'
if rg -n "$forbidden" "${files[@]}"; then
  echo "Layout viewer interaction policy contains a forbidden GUI/application dependency." >&2
  exit 1
fi

echo "Layout viewer interaction boundary check passed."
