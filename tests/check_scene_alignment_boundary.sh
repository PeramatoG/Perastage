#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if rg -n '#include.*(wx/|GL/|viewer|gui/)|ConfigManager|wxWindow' core/alignment_geometry.* core/scene_alignment.*; then
  echo 'Scene alignment geometry and command must remain GUI independent.' >&2
  exit 1
fi
rg -q 'BuildInteractiveTransformTargets' core/scene_alignment.cpp
rg -q 'ExecuteBatch' core/scene_alignment.cpp
rg -q 'alignment::Execute' gui/mainwindow_alignment.cpp
rg -q 'physicalBounds' core/scene_alignment_bounds.cpp
