#!/usr/bin/env python3
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'tests/check_perastage_tree_modules.sh'
with tempfile.TemporaryDirectory() as tmp:
    bin_dir = Path(tmp) / 'bin'
    bin_dir.mkdir()
    for tool in ['bash', 'dirname']:
        os.symlink(f'/usr/bin/{tool}', bin_dir / tool)
    env = {**os.environ, 'PATH': str(bin_dir)}
    result = subprocess.run(['/usr/bin/bash', str(SCRIPT)], env=env, text=True, capture_output=True)
    assert result.returncode == 127, result.stdout + result.stderr
    assert "required test tool 'rg'" in result.stderr, result.stdout + result.stderr
print('OK: ripgrep-dependent policy scripts fail clearly when rg is missing.')
