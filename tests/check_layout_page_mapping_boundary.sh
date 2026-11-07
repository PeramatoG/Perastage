#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
run_test_python - <<'PY'
from pathlib import Path
orchestrator = Path('gui/mainwindow_print.cpp').read_text().split('void MainWindow::OnPrintLayout')[1].split('void MainWindow::OnPrintTable')[0]
assert 'lround' not in orchestrator and 'settings.SaveToConfig' not in orchestrator
assert 'data.frame = view.frame;' in orchestrator
text = Path('gui/layouttextutils.cpp').read_text().split('LayoutTextExportData BuildLayoutTextExportData')[1].split('wxImage RenderTextImage')[0]
assert 'scaleX' not in text and 'scaleY' not in text and 'data.frame = text.frame;' in text
image = Path('gui/layout_image_export.cpp').read_text()
assert 'data.frame = imageDef.frame;' in image
assert 'ConfigManager' not in image
for name in ('legend','event_table','text'):
    adapter = Path('viewer2d/pdf/layout_pdf_' + name + '_renderer.cpp').read_text()
    assert 'ScopedDocumentPageMapping pageMapping' in adapter
exporter = Path('viewer2d/pdf/layout_pdf_exporter.cpp').read_text()
assert exporter.count('ScopedDocumentPageMapping pageMapping') == 2 # View and Image.
assert 'ScopedDocumentPageClip pageClip' in exporter
assert '* layoutScale' not in Path('viewer2d/pdf/layout_pdf_text_renderer.cpp').read_text()
print('Layout page mapping boundary passed.')
PY
