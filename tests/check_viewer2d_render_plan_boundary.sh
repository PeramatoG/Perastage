#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
files=(
  "$repo_root/viewer2d/render/viewer2d_render_frame_plan.h"
  "$repo_root/viewer2d/render/viewer2d_render_frame_plan.cpp"
)

for token in 'wx/' 'GL/' 'ConfigManager' 'Viewer2DPanel' 'MainWindow' \
             'TablePanel' 'Viewer3DController'; do
  if rg -n "$token" "${files[@]}"; then
    echo "Viewer2D render planning must remain GUI and renderer independent: $token" >&2
    exit 1
  fi
done

echo "Viewer2D render-plan boundary checks passed."
