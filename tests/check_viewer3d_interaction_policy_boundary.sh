#!/usr/bin/env bash
set -euo pipefail

files=(
  viewer3d/interaction/navigation_interaction_policy.h
  viewer3d/interaction/navigation_interaction_policy.cpp
  viewer3d/interaction/selection_drag_activation.h
  viewer3d/interaction/selection_drag_activation.cpp
  viewer3d/interaction/selection_interaction_policy.h
  viewer3d/interaction/selection_interaction_policy.cpp
  viewer3d/interaction/scene_selection_policy.h
  viewer3d/interaction/scene_selection_policy.cpp
)

for forbidden in \
  '#include[[:space:]]*[<"]wx' \
  '#include[[:space:]]*[<"][^">]*viewer3dpanel' \
  '#include[[:space:]]*[<"][^">]*gui/' \
  '#include[[:space:]]*[<"][^">]*(fixture|truss|hoist|sceneobject)tablepanel' \
  '#include[[:space:]]*[<"][^">]*(dialog|mainwindow)'; do
  if rg -n -i "$forbidden" "${files[@]}"; then
    echo "Viewer3D interaction policies must not depend on wxWidgets or GUI adapters." >&2
    exit 1
  fi
done

if rg -n '\b(wx(Window|Point|MouseEvent|KeyEvent|Menu|GLCanvas)|Viewer3DPanel|MainWindow)\b' "${files[@]}"; then
  echo "Viewer3D interaction policies must not depend on wxWidgets or GUI panels." >&2
  exit 1
fi

echo "Viewer3D interaction policy boundary check passed."
