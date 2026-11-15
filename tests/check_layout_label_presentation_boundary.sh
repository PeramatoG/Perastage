#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep
cd "$(dirname "${BASH_SOURCE[0]}")/.."
if rg -n '#include.*(wx/|GL/|nanovg)' core/layouts/FixtureLabelPresentation*.h viewer2d/fixture_label_presentation.*; then
  echo 'Label presentation resolution must stay independent of GUI and OpenGL.' >&2
  exit 1
fi
if rg -n '(SetValue|SaveFixtureLabelOverrides|CommitFixtureLabelStyles)' viewer2d/fixture_label_presentation.cpp viewer2d/viewer2dstate.cpp gui/layout_2d_view_capture_service.cpp gui/mainwindow_print.cpp; then
  echo 'Viewport rendering must not replace authoritative project label metadata.' >&2
  exit 1
fi
rg -q 'GetFixtureLabelPresentation.*styles' viewer3d/labels/label_render_system.cpp
rg -q 'BuildLabelPresentationContext' viewer2d/viewer2d_label_presentation.cpp
rg -q 'LabelPresentationJson' gui/layoutviewerpanel_render_invalidation.cpp
rg -q 'LabelFieldsJson' gui/layoutviewerpanel_render_invalidation.cpp
echo 'Layout label presentation boundary passed.'

if rg -n 'SetSelectedFixtures|SetSelectedTrusses|PushUndoState|MarkDirty|transform\.o[[:space:]]*=' gui/layout_fixture_label_selection.cpp viewer2d/viewer2d_layout_label_selection.cpp; then
  echo 'Layout label target picking must not mutate project selection, transforms or history.' >&2
  exit 1
fi

rg -q 'kFixtureLabelCompositionRevision' gui/layoutviewerpanel_render_invalidation.cpp
if rg -n 'BuildFixtureLayoutEntry|BuildFixtureLayoutKey' viewer3d/labels/label_render_system.cpp; then
  echo '2D fixture labels must not keep a separate legacy font/layout path.' >&2
  exit 1
fi
