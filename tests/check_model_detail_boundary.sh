#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
run_test_python - <<'PYTHON'
from pathlib import Path
policy = Path('core/model_detail_policy.h').read_text()
for forbidden in ['wx/', 'meshoptimizer.h', 'gdtf_geometry_types.h']:
    assert forbidden not in policy, f'Model detail policy must be GUI/resource independent: {forbidden}'
for path in ['gui/preferencesdialog.cpp', 'gui/preferences/viewer3d_rendering_preferences_panel.cpp']:
    text = Path(path).read_text()
    assert 'meshopt_' not in text and 'ResolvePolicy(' not in text, 'GUI must only present/load/apply policy'
fixture = Path('viewer3d/render/opaque_fixture_pass.cpp').read_text()
assert 'meshopt_' not in fixture and 'proxyCache' not in fixture, 'Simplification/cache belongs to resources'
assert 'ResolveRenderMesh' in fixture
for path in ['viewer3d/render/opaque_object_pass.cpp', 'viewer3d/render/opaque_truss_pass.cpp']:
    assert 'ResolveRenderMesh' in Path(path).read_text()
for key in ['model_detail::kDetailConfigKey', 'model_detail::kMovingProxyConfigKey']:
    for path in ['core/configmanager.cpp', 'gui/mainwindow.cpp']:
        assert key in Path(path).read_text(), f'{path} must preserve {key}'
assert 'preservedModelDetail.Restore(cfg)' in Path('gui/mainwindow/controllers/mainwindow_io_controller.cpp').read_text()
print('OK: model detail ownership and user preference bookkeeping')
PYTHON
