#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
run_test_python - <<'PY'
from pathlib import Path
core = Path('core/layouts/PageOutputMapping.h').read_text() + ''.join(Path('core/layouts/EventTableComposition.' + ext).read_text() for ext in ('h','cpp'))
for forbidden in ('#include <wx', 'ConfigManager', 'OpenGL', 'filesystem'):
    assert forbidden not in core, forbidden
for path in ('gui/layout_event_table_renderer.cpp', 'viewer2d/pdf/layout_pdf_event_table_renderer.cpp'):
    adapter = Path(path).read_text()
    assert 'ComposeEventTable(' in adapter
    for forbidden in ('GetTextExtent', 'MeasureTextWidth', 'trimText', 'TypographyScale', 'rowHeight ='):
        assert forbidden not in adapter, (path, forbidden)
for name in ('legend', 'event_table'):
    assert 'ScopedDocumentPageMapping ' in Path('viewer2d/pdf/layout_pdf_' + name + '_renderer.cpp').read_text()
source = Path('gui/mainwindow_print.cpp').read_text().split('for (const auto &table : layout->eventTables)')[1].split('for (const auto &text')[0]
assert 'lround' not in source and 'tableData.frame = table.frame;' in source
assert 'kEventTableCompositionVersion' in Path('gui/layout_event_table_cache_policy.h').read_text()
print('Event Table composition boundary passed.')
PY
