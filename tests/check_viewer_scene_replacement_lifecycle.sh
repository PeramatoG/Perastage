#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
main_window="$repo_root/gui/mainwindow.cpp"
controller="$repo_root/viewer3d/viewer3dcontroller.cpp"

prepare_line="$(rg -n -m1 'viewportPanel->PrepareForSceneReplacement\(\)' "$main_window" | cut -d: -f1)"
load_line="$(rg -n -m1 'LegacyConfigManager\(\)\.LoadProject\(' "$main_window" | cut -d: -f1)"
complete_line="$(rg -n -m1 'viewportPanel->CompleteSceneReplacement\(\)' "$main_window" | cut -d: -f1)"

if [[ -z "$prepare_line" || -z "$load_line" || -z "$complete_line" ]] ||
   (( prepare_line >= load_line || complete_line <= load_line )); then
  echo "Project loading must bracket scene replacement with viewer cache lifecycle calls." >&2
  exit 1
fi

rg -q 'sceneReplacementActive\.store\(true' "$controller"
rg -q 'sceneReplacementActive\.store\(false' "$controller"
if (( $(rg -c 'sceneReplacementActive\.load' "$controller") < 2 )); then
  echo "Viewer rendering and synchronization must pause during scene replacement." >&2
  exit 1
fi

echo "OK: project loading invalidates viewer scene references before replacing scene data."
