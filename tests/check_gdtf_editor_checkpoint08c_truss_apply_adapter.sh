#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
adapter_h = Path('core/gdtf/editor/project_truss_gdtf_apply_adapter.h')
adapter_cpp = Path('core/gdtf/editor/project_truss_gdtf_apply_adapter.cpp')
assert adapter_h.exists() and adapter_cpp.exists(), 'Project Truss adapter files must exist'
cmake = Path('core/CMakeLists.txt').read_text() + Path('tests/CMakeLists.txt').read_text()
assert 'project_truss_gdtf_apply_adapter.cpp' in cmake, 'Truss adapter must be registered in CMake'
adapter = adapter_h.read_text() + adapter_cpp.read_text()
for token in ['GdtfApplyRequest', 'GdtfApplyResult']:
    assert token in adapter, f'adapter must use {token}'
for forbidden in ['wx', 'TrussEditDialog', 'TrussTablePanel', 'ConfigManager', 'Viewer2D', 'Viewer3D', 'MessageBox']:
    assert forbidden not in adapter, f'adapter has forbidden dependency {forbidden}'
truss = Path('gui/trusseditdialog.cpp').read_text()
for token in ['project_truss_gdtf_apply_adapter.h', 'ProjectTrussGdtfApplyAdapter', 'BuildApplyRequest()', '.Apply(', 'common.success']:
    assert token in truss, f'TrussEditDialog missing {token}'
for forbidden in ['EnsureGdtfForEditedTruss', 'BuildTrussGdtfFromInstance']:
    assert forbidden not in truss, f'TrussEditDialog still uses legacy generation {forbidden}'
fixture = Path('gui/fixtureeditdialog.cpp').read_text()
for token in ['ProjectFixtureGdtfApplyAdapter', 'BuildApplyRequest()', 'common.success']:
    assert token in fixture, f'Fixture 08B integration missing {token}'
for forbidden in ['CreateOrUpdatePerastageLibraryDerivative', 'SetGdtfProperties', 'ApplySharedPhysicalPropertyEdit', 'ApplyModeForGdtf']:
    assert forbidden not in fixture, f'FixtureEditDialog still uses migrated legacy operation {forbidden}'
test = Path('tests/project_truss_gdtf_apply_adapter_test.cpp').read_text()
for token in ['generationOccurred', 'calls == 0', 'stableHostId', 'ProjectControlledGeneration']:
    assert token in test, f'Truss adapter test missing invariant {token}'
PY
