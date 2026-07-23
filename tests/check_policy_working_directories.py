#!/usr/bin/env python3
from pathlib import Path
import argparse
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--bash', required=True)
args = parser.parse_args()
BASH = args.bash
SCRIPTS = [
    ROOT / 'tests/check_securestore_build_policy.sh',
    ROOT / 'tests/check_ci_cmake_language_policy.sh',
]

with tempfile.TemporaryDirectory() as tmp:
    build = ROOT / 'build' / 'policy-working-directory-regression'
    build.mkdir(parents=True, exist_ok=True)
    for cwd in [ROOT, build, Path(tmp)]:
        for script in SCRIPTS:
            env = {**os.environ, 'PERASTAGE_TEST_PYTHON': os.environ.get('PERASTAGE_TEST_PYTHON', sys.executable)}
            result = subprocess.run([BASH, str(script)], cwd=cwd, env=env, text=True, capture_output=True)
            assert result.returncode == 0, f'{script.name} failed from {cwd}:\n{result.stdout}\n{result.stderr}'
print('OK: representative policy scripts resolve repository paths from root, build, and temporary working directories.')
