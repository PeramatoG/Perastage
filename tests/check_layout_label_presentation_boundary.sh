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
