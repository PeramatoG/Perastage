#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = [
    ROOT / 'tests/check_securestore_build_policy.sh',
    ROOT / 'tests/check_ci_cmake_language_policy.sh',
]

with tempfile.TemporaryDirectory() as tmp:
    build = ROOT / 'build' / 'policy-working-directory-regression'
    build.mkdir(parents=True, exist_ok=True)
    for cwd in [ROOT, build, Path(tmp)]:
        for script in SCRIPTS:
            result = subprocess.run(['bash', str(script)], cwd=cwd, text=True, capture_output=True)
            assert result.returncode == 0, f'{script.name} failed from {cwd}:\n{result.stdout}\n{result.stderr}'
print('OK: representative policy scripts resolve repository paths from root, build, and temporary working directories.')
