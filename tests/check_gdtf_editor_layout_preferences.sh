#!/usr/bin/env bash
set -euo pipefail

python3 - <<'PY'
from pathlib import Path
source = Path('gui/gdtf/gdtf_editor_layout_preferences.cpp').read_text()
header = Path('gui/gdtf/gdtf_editor_layout_preferences.h').read_text()

def require(condition, message):
    if not condition:
        raise SystemExit(message)

for token in ['ClampDialogSize', 'ClampSplitterRatio']:
    require(token in header and token in source, f'Missing layout preference clamp helper: {token}.')
metrics = Path('gui/gdtf/gdtf_editor_visual_metrics.h').read_text()
for token in ['RatioToSash', 'SashToRatio']:
    require(token in metrics, f'Missing shared splitter conversion helper: {token}.')
require('std::strtod' in source and 'std::strtol' in source,
        'Layout preferences must ignore malformed persisted numbers safely.')
require('std::clamp(ratio, 0.15, 0.85)' in metrics,
        'Splitter ratio clamp must prevent fully collapsed panes.')
require('display.GetClientArea()' in source and 'ClampDialogSize(' in source,
        'Dialog restore sizes must be clamped to the current display work area.')
fixture_keys = [
    'gdtf_editor/fixture/dialog_width',
    'gdtf_editor/fixture/dialog_height',
    'gdtf_editor/fixture/context_ratio',
    'gdtf_editor/fixture/visual_ratio',
    'gdtf_editor/fixture/gdtf_ratio',
    'gdtf_editor/fixture/visual_tab',
]
truss_keys = [
    'gdtf_editor/truss/dialog_width',
    'gdtf_editor/truss/dialog_height',
    'gdtf_editor/truss/context_ratio',
    'gdtf_editor/truss/preview_ratio',
    'gdtf_editor/truss/gdtf_ratio',
]
for key in fixture_keys + truss_keys:
    require(key in source, f'Missing persisted layout key: {key}.')
require(not set(fixture_keys) & set(truss_keys),
        'Fixture and Truss preference keys must remain independent.')
require('std::clamp(ReadInt(config, kFixtureVisualTab, 0), 0, 1)' in source,
        'Fixture visual tab restore must clamp to the two current tabs.')
require('static_cast<int>(area.GetHeight() * 0.65)' in source and 'Dip(window, 560)' in source,
        'Truss Edit default and minimum heights must stay compact.')
require('config.SaveUserConfig();' in source,
        'Layout preference saves must flush through the existing configuration abstraction.')
print('OK: GDTF editor layout preference helper checks passed.')
PY
