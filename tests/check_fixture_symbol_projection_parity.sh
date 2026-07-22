#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"

run_test_python - <<'PY'
from pathlib import Path

fixture = Path('viewer3d/render/opaque_fixture_pass.cpp').read_text()
builder = Path('viewer3d/render/perastage_svg_symbol_builder.cpp').read_text()
preview = Path('gui/layoutviewerviewrenderer.cpp').read_text()
pdf = Path('viewer2d/pdf/layout_pdf_exporter.cpp').read_text()
cache = Path('gui/layoutviewerpanel_render_invalidation.cpp').read_text()

assert 'ResolveFixtureSymbolProjection(fixtureTransform, fixtureCaptureView,' in fixture, 'fixture capture must use the shared projection resolver'
assert 'symbolKey.viewKind = symbolProjection.symbolView' in fixture, 'symbol key must use the resolved symbol view'
assert 'symbolProjection.instanceTransform' in fixture, 'instance placement must use the resolved transform'
assert 'svg.viewKind == SymbolViewKind::Front' in builder and 'svg.viewKind == SymbolViewKind::Left' in builder, 'fallback SVG anchor logic must use the loaded view kind'
assert 'RenderCommandBuffer(dc, it->second.localCommands' in preview, 'layout preview must replay captured local symbol commands'
assert 'defIt->second.localCommands.commands' in pdf, 'layout PDF export must replay captured local symbol commands'
assert 'ResolveFixtureSymbolProjection' not in pdf, 'PDF export must not reselect fixture symbol projection independently'
assert 'kLayoutFixtureSymbolProjectionVersion = 2' in cache, 'layout capture hash must include the projection algorithm version'
print('OK: fixture symbol projection parity guard passed.')
PY
