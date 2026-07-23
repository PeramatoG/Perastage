#!/usr/bin/env python3
from pathlib import Path
import json
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / '.github/scripts/summarize_ctest_results.py'

with tempfile.TemporaryDirectory() as tmp:
    base = Path(tmp)
    junit = base / 'ctest.junit.xml'
    out = base / 'results.json'
    junit.write_text('''<testsuite tests="5" failures="1" errors="1" skipped="1" disabled="1">
<testcase name="Passed" />
<testcase name="Failed"><failure message="failed" /></testcase>
<testcase name="Errored"><error message="error" /></testcase>
<testcase name="Skipped"><skipped message="skipped" /></testcase>
<testcase name="Disabled" />
</testsuite>''')
    subprocess.run([
        sys.executable, str(TOOL), '--platform', 'linux', '--junit', str(junit),
        '--output', str(out), '--tested-sha', 'abc123', '--profile', 'pr', '--labels', 'release-gate,policy'
    ], check=True)
    data = json.loads(out.read_text())
    assert data['total'] == 5, data
    assert data['passed'] == 1, data
    assert data['failed'] == 2, data
    assert data['skipped'] == 1, data
    assert data['disabled_not_run'] == 1, data
    assert data['tested_sha'] == 'abc123', data
    assert data['selected_labels'] == ['release-gate', 'policy'], data
print('OK: CTest result summary writes compact counts and selection metadata.')
