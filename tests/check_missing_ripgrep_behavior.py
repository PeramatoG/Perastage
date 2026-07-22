#!/usr/bin/env python3
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--bash', required=True)
args = parser.parse_args()
BASH = args.bash
SCRIPT = ROOT / 'tests/check_perastage_tree_modules.sh'
with tempfile.TemporaryDirectory() as tmp:
    bin_dir = Path(tmp) / 'bin'
    bin_dir.mkdir()
    os.symlink('/usr/bin/dirname', bin_dir / 'dirname')
    env = {**os.environ, 'PATH': str(bin_dir)}
    result = subprocess.run([BASH, str(SCRIPT)], env=env, text=True, capture_output=True)
    assert result.returncode == 127, result.stdout + result.stderr
    assert "required test tool 'rg'" in result.stderr, result.stdout + result.stderr
print('OK: ripgrep-dependent policy scripts fail clearly when rg is missing.')
