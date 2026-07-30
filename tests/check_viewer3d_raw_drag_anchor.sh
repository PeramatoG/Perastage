#!/usr/bin/env bash
set -euo pipefail

source_file="viewer3d/viewer3dpanel.cpp"
block="$(sed -n '/void Viewer3DPanel::OnMouseMove/,/void Viewer3DPanel::OnMouseWheel/p' "$source_file")"

if [[ "$(printf '%s' "$block" | rg -c 'renderSize, rawAnchor')" -lt 2 ]]; then
  echo "Expected unconstrained pointer projections to use the raw anchor." >&2
  exit 1
fi
if printf '%s' "$block" | rg -q 'renderSize, m_selectionDragAnchorMeters'; then
  echo "Pointer projections must not use the snapped selection anchor directly." >&2
  exit 1
fi
