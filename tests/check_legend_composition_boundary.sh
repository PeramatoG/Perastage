#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
run_test_python - <<'PY'
from pathlib import Path
core = Path('core/layouts/LegendComposition.cpp').read_text()
contract = Path('core/layouts/LegendComposition.h').read_text() + Path('core/layouts/DocumentComposition.h').read_text()
for forbidden in ('#include <wx', '#include "wx', 'ConfigManager', 'OpenGL', 'filesystem', 'LoadUsableFixtureSymbol'):
    assert forbidden not in core + contract, forbidden
preview = Path('gui/layoutlegendrenderer.cpp').read_text()
pdf = Path('viewer2d/pdf/layout_pdf_legend_renderer.cpp').read_text()
for adapter in (preview, pdf):
    assert 'ComposeLegend(input,' in adapter
    for duplicate in ('wrapTextToTwoLines', 'typeWidthLogical', 'maxCountWidth', 'rowHeightCandidate'):
        assert duplicate not in adapter, duplicate
assert 'LegacyLegendMetrics' not in core + contract
assert 'GetTextExtent' not in preview
assert 'kPdfLayoutTypographyCompensationScale' not in pdf
assert 'frame.width * scaleX' not in Path('gui/mainwindow_print.cpp').read_text().split('for (const auto &legend : layout->legendViews)')[1].split('for (const auto &table')[0]
refresh = Path('gui/layoutviewerpanel_legend.cpp').read_text()
assert refresh.index('InvalidateChangedLegendRasters(') < refresh.index('newHash == legendDataHash')
assert 'entry.second.renderDirty = true' not in refresh
assert 'legend-composition-v2-perastage-sans' in refresh
print('Legend composition boundary passed.')
PY
