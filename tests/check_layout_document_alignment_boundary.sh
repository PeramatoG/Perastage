#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if rg -n '#include.*(wx/|GL/|gui/|scene_alignment)|WorldDelta|SceneBounds|ConfigManager|LayoutManager' core/layouts/LayoutDocumentEdit.* core/layouts/LayoutElementRef.h; then
  echo 'Layout document geometry must remain independent of GUI and scene transforms.' >&2
  exit 1
fi
rg -q 'alignment::Calculate' core/layouts/LayoutDocumentEdit.cpp
rg -q 'ApplyMutation' core/layouts/LayoutManagerDocumentEdit.cpp
rg -q 'ResolveViewportFrame' core/layouts/LayoutDocumentEdit.h
rg -q 'SetViewportFrame' core/layouts/LayoutDocumentEdit.h
rg -q 'AlignSelectedElements' gui/mainwindow_alignment.cpp
