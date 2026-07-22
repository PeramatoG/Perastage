#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
cd "$(dirname "$0")/.."
run_test_python - <<'PY'
from pathlib import Path
src = Path('gui/fixtureeditdialog.cpp').read_text()
assert '#include "gdtf/editor/project_fixture_gdtf_apply_adapter.h"' in src
for token in ['ProjectFixtureGdtfApplyAdapter', 'BuildApplyRequest()', '.Apply(', 'common.success']:
    assert token in src, f'missing {token}'
for forbidden in ['CreateOrUpdatePerastageLibraryDerivative', 'SetGdtfProperties', 'ApplySharedPhysicalPropertyEdit', 'ApplyModeForGdtf']:
    assert forbidden not in src, f'FixtureEditDialog still uses migrated legacy operation {forbidden}'
accept = src.find('BuildEditSession();', src.find('ProjectFixtureGdtfApplyAdapter'))
success = src.find('common.success', src.find('ProjectFixtureGdtfApplyAdapter'))
assert success != -1 and accept != -1 and success < accept, 'session rebuild must occur after success check'
PY
