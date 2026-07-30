#!/usr/bin/env bash
set -euo pipefail

source_file="viewer3d/viewer3dpanel.cpp"
block="$(sed -n '3505,3522p' "$source_file")"

if [[ "$(printf '%s' "$block" | rg -c 'ProjectMouseToSelectionDragViewPlane')" -ne 2 ]]; then
  echo "Expected both unconstrained pointer projections in the checked block." >&2
  exit 1
fi
if [[ "$(printf '%s' "$block" | rg -c 'renderSize, rawAnchor')" -ne 2 ]]; then
  echo "Unconstrained pointer projections must share CurrentRawSelectionDragAnchor()." >&2
  exit 1
fi
