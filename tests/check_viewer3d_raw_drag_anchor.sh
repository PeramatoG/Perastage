#!/usr/bin/env bash
set -euo pipefail

source_file="viewer3d/viewer3dpanel.cpp"
selection="$(sed -n '/void Viewer3DPanel::UpdateSelectionDragPointer/,/void Viewer3DPanel::OnMouseMove/p' "$source_file")"
placement="$(sed -n '/void Viewer3DPanel::UpdateContinuousPlacementPointer/,$p' viewer3d/viewer3dpanel_presentation.cpp)"
block="$selection"$'
'"$placement"

if [[ "$(printf '%s' "$block" | rg -c 'renderSize, rawAnchor')" -lt 2 ]]; then
  echo "Expected unconstrained pointer projections to use the raw anchor." >&2
  exit 1
fi
if printf '%s' "$block" | rg -q 'renderSize, m_selectionDragAnchorMeters'; then
  echo "Pointer projections must not use the snapped selection anchor directly." >&2
  exit 1
fi
