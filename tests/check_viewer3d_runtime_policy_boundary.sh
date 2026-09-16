#!/usr/bin/env bash
set -euo pipefail

files=(
  viewer3d/interaction/viewer_runtime_policy.h
  viewer3d/interaction/viewer_runtime_policy.cpp
)

for forbidden in \
  '#include[[:space:]]*[<"]wx' \
  '#include[[:space:]]*[<"](GL|OpenGL|[^">]*/GL)/' \
  '#include[[:space:]]*[<"][^">]*(configmanager|viewer3dpanel|viewer3dcontroller)' \
  '#include[[:space:]]*[<"][^">]*gui/' \
  '#include[[:space:]]*[<"][^">]*(fixture|truss|hoist|sceneobject)tablepanel' \
  '#include[[:space:]]*[<"][^">]*(dialog|mainwindow)'; do
  if rg -n -i "$forbidden" "${files[@]}"; then
    echo "Viewer3D runtime policy must remain independent of GUI and rendering adapters." >&2
    exit 1
  fi
done

if rg -n '\b(wx(Point|ThreadEvent|GLCanvas)|Viewer3DPanel|Viewer3DController|ConfigManager|MainWindow|gl[A-Z]|GL[A-Z]|glew)' "${files[@]}" || \
   rg -n '\bRefresh[[:space:]]*\(' "${files[@]}"; then
  echo "Viewer3D runtime policy must not execute GUI, controller, or refresh work." >&2
  exit 1
fi

echo "Viewer3D runtime policy boundary check passed."
