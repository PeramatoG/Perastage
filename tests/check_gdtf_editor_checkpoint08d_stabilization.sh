#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
import re

def body(path, name):
    text = Path(path).read_text()
    m = re.search(r'(?:bool|[\w:]+)\s+' + re.escape(name) + r'\([^)]*\)\s*\{', text)
    assert m, f'{name} not found in {path}'
    i = m.end(); depth = 1
    while i < len(text) and depth:
        if text[i] == '{': depth += 1
        elif text[i] == '}': depth -= 1
        i += 1
    return text[m.end():i-1]

adapter_h = Path('core/gdtf/editor/project_fixture_gdtf_apply_adapter.h').read_text()
assert 'const std::unordered_map<std::string, Fixture> *fixtures' in adapter_h
assert 'updatedFixtures' in adapter_h
adapter_cpp = Path('core/gdtf/editor/project_fixture_gdtf_apply_adapter.cpp').read_text()
assert '*input.fixtures =' not in adapter_cpp
assert 'result.updatedFixtures' in adapter_cpp
assert 'fixtureType' in Path('gui/fixture_gdtf_apply_services.cpp').read_text()
assert 'CreateOrUpdatePerastageLibraryDerivative(\n        fixtureType' in Path('gui/fixture_gdtf_apply_services.cpp').read_text()
assert '.string()' not in Path('gui/fixture_gdtf_apply_services.cpp').read_text()

for path, name in [('gui/fixtureeditdialog.cpp','FixtureEditDialog::ApplyChanges'),('gui/trusseditdialog.cpp','TrussEditDialog::ApplyChanges')]:
    b = body(path, name)
    apply_pos = b.find('.Apply(')
    set_pos = b.find('SetValue(')
    assert apply_pos >= 0, f'{path} must invoke adapter before committing table state'
    assert set_pos < 0 or apply_pos < set_pos, f'{path} writes table before adapter success'
    if 'fixture' in path:
        cache_pos = b.find('gdtfPaths')
        assert cache_pos < 0 or apply_pos < cache_pos, 'Fixture path cache is updated before adapter success'
        assert 'updatedFixtures' in b and 'UpdateSceneData(true' in b
    else:
        assert 'PushUndoState(\n        "edit truss")' in b

for path in ['core/gdtf/editor/project_fixture_gdtf_apply_adapter.cpp','core/gdtf/editor/project_truss_gdtf_apply_adapter.cpp']:
    text = Path(path).read_text()
    forbidden = ['wx', 'ConfigManager', 'DataView', 'Viewer2D', 'Viewer3D', 'wxMessageBox']
    assert not any(token in text for token in forbidden), f'{path} must remain non-GUI'

code_text = '\n'.join(Path(p).read_text(errors='ignore') for p in Path('.').rglob('*') if p.is_file() and p.suffix in {'.cpp','.h','.hpp','.cc'})
assert 'StartupRouter' not in code_text
print('OK: GDTF editor Checkpoint 08D stabilization checks passed.')
PY
bash tests/check_gdtf_editor_checkpoint08a_binding.sh >/dev/null
bash tests/check_gdtf_editor_checkpoint08b_fixture_apply_adapter.sh >/dev/null
bash tests/check_gdtf_editor_checkpoint08c_truss_apply_adapter.sh >/dev/null
