#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
run_test_python - <<'PY'
from pathlib import Path
core = Path('core/layouts/PhysicalViewport.h').read_text()
assert not any(token in core for token in ('#include <wx', 'OpenGL', 'ConfigManager'))
assert '1000.0 / v.denominator * 72.0 / 25.4' in core
for filename in ('gui/layout_2d_view_capture_service.cpp', 'gui/mainwindow_print.cpp'):
    source = Path(filename).read_text()
    assert 'PreparePhysicalCapture' in source and 'AttachPhysicalMapping' in source
assert 'PreparePhysicalCapture' in Path('gui/layout_2d_view_rasterizer.cpp').read_text()
assert 'Viewer2DCommandRenderer renderer' in Path('gui/layout_viewport_preview.cpp').read_text()
assert 'ValidatePhysicalOutput' in Path('viewer2d/pdf/layout_pdf_view_mapping.h').read_text()
assert 'ViewportScaleMode scaleMode = ViewportScaleMode::Free' in Path('core/layouts/LayoutCollection.h').read_text()
editor = Path('gui/mainwindow_layout.cpp').read_text()
passive = editor.split('void MainWindow::PersistLayout2DViewState()')[1].split('// Restores the selected layout')[0]
assert 'UpdateLayout2DView' not in passive
assert 'ScopedPreferenceDraft preferenceDraft' in editor
for filename in ('gui/layoutviewerpanel_view.cpp', 'gui/layoutviewerpanel_render_invalidation.cpp'):
    assert 'GetFrameRect(layouts::ResolveViewportFrame(view)' in Path(filename).read_text()
assert 'viewportState_.FrameRect' in Path('gui/layoutviewerpanel_viewport_geometry.cpp').read_text()
policy = Path('gui/ui_print_build_defaults.h').read_text()
assert not any(token in policy for token in ('#include <wx', 'OpenGL', 'ConfigManager', 'NDEBUG'))
assert 'detail::ApplyPrintBuildDefaults' in Path('gui/ui_feature_flags.cpp').read_text()
print('Layout physical viewport boundary passed.')
PY
