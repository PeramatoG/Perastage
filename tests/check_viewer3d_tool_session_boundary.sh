#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
files=(
  "$root/viewer3d/interaction/selection_drag_session.h"
  "$root/viewer3d/interaction/selection_drag_session.cpp"
  "$root/viewer3d/interaction/line_point_selection_session.h"
  "$root/viewer3d/interaction/line_point_selection_session.cpp"
  "$root/models/continuous_placement_state.h"
  "$root/models/continuous_placement_state.cpp"
)

forbidden='wxWidgets|wx/|wx(Point|MouseEvent|Window)|GL/|OpenGL/|glew|Viewer3DPanel|Viewer3DController|ConfigManager|TablePanel|MainWindow|HistoryManager|(^|[^[:alnum:]_])Refresh\('
if rg -n "$forbidden" "${files[@]}"; then
  echo "Viewer3D tool session boundary contains an adapter dependency." >&2
  exit 1
fi

echo "Viewer3D tool session boundary check passed."

stale_panel_state='m_continuousPlacementActive|m_linePointSelectionActive|m_measureToolEnabled|m_placementViewRevision'
if rg -n "$stale_panel_state" "$root/viewer3d" \
    --glob 'viewer3dpanel*.cpp' --glob 'viewer3dpanel*.h'; then
  echo "Viewer3DPanel implementation contains removed session-state members." >&2
  exit 1
fi

echo "Viewer3DPanel session-state migration check passed."
