#!/usr/bin/env bash
set -euo pipefail

files=(
  viewer3d/interaction/navigation_interaction_policy.h
  viewer3d/interaction/navigation_interaction_policy.cpp
  viewer3d/interaction/selection_interaction_policy.h
  viewer3d/interaction/selection_interaction_policy.cpp
  viewer3d/interaction/scene_selection_policy.h
  viewer3d/interaction/scene_selection_policy.cpp
)

if rg -n '#include[[:space:]]*[<"](wx|.*viewer3dpanel|gui/)' "${files[@]}"; then
  echo "Viewer3D interaction policies must not depend on wxWidgets or GUI panels." >&2
  exit 1
fi

echo "Viewer3D interaction policy boundary check passed."
