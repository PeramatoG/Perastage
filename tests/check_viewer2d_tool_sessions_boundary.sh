#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
files=(
  "$root/viewer2d/interaction/viewer2d_placement_session.h"
  "$root/viewer2d/interaction/viewer2d_placement_session.cpp"
  "$root/viewer2d/interaction/viewer2d_line_point_selection_session.h"
  "$root/viewer2d/interaction/viewer2d_line_point_selection_session.cpp"
)

forbidden='wxWidgets|wx/|OpenGL|GL/|glew|Viewer2DPanel|Viewer3D|MainWindow|ConfigManager|MvrScene|TablePanel|HistoryManager|scene_clipboard|magnet_snap|Refresh\('
if rg -n "$forbidden" "${files[@]}"; then
  echo "Viewer2D tool-session boundary contains an adapter dependency." >&2
  exit 1
fi

panel_header="$root/viewer2d/viewer2dpanel.h"
legacy_fields='m_continuousPlacementActive|m_continuousPlacementType|m_continuousPlacementUuid|m_continuousPlacedUuids|m_clipboardBatchPlacement|m_linePointSelectionActive|m_linePointSelectionConsumeMouseUp|m_linePointSelection(Start|End|First|Preview)'
if rg -n "$legacy_fields" "$panel_header"; then
  echo "Viewer2DPanel still duplicates extracted tool-session state." >&2
  exit 1
fi

echo "Viewer2D tool-session boundary check passed."
