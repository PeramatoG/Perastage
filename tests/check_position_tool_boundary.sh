#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$root/tests/test_tool_requirements.sh"
require_ripgrep
if rg -n 'wx[A-Z]|configmanager|glBegin|Canvas' "$root/core/position_selection.h" "$root/core/position_selection.cpp"; then
  echo 'Position selection must stay GUI independent.' >&2
  exit 1
fi
if rg -n 'SetTargetWorldTransform|\.transform[[:space:]]*=' "$root/gui/positiontoolpanel.cpp"; then
  echo 'Position Tool must delegate scene mutation to Core.' >&2
  exit 1
fi
if rg -n 'CaptureCanvas|recordingCanvas|Emit.*Canvas' "$root/viewer2d/viewer2dpanel_position_overlay.cpp"; then
  echo 'Insertion-point overlays must not enter capture commands.' >&2
  exit 1
fi
rg -q 'transform::Execute' "$root/core/position_selection.cpp"
rg -q 'WorldToFramebuffer' "$root/viewer2d/position_insertion_overlay.cpp"
rg -q 'ShowInsertionPointOverlay' "$root/viewer2d/viewer2dpanel_position_overlay.cpp"
echo 'Position Tool boundary check passed.'
