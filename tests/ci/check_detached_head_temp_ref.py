#!/usr/bin/env python3
"""Verify release temp refs are pushed and deleted with explicit full refspecs."""

from pathlib import Path
import subprocess
import tempfile


def run(args, cwd, check=True):
    return subprocess.run(args, cwd=cwd, check=check, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


with tempfile.TemporaryDirectory(prefix='perastage-temp-ref-') as root:
    root_path = Path(root)
    remote = root_path / 'remote.git'
    source = root_path / 'source'
    full_ref = 'refs/heads/automation/release-v1.5.0-test'

    run(['git', 'init', '--bare', str(remote)], cwd=root_path)
    run(['git', 'init', str(source)], cwd=root_path)
    run(['git', 'config', 'user.name', 'Perastage CI Test'], cwd=source)
    run(['git', 'config', 'user.email', 'ci-test@example.invalid'], cwd=source)
    run(['git', 'remote', 'add', 'origin', str(remote)], cwd=source)

    (source / 'VERSION').write_text('1.5.0\n')
    run(['git', 'add', 'VERSION'], cwd=source)
    run(['git', 'commit', '-m', 'test release ref'], cwd=source)
    commit = run(['git', 'rev-parse', 'HEAD'], cwd=source).stdout.strip()
    run(['git', 'checkout', '--detach', commit], cwd=source)

    previous = run(['git', 'push', 'origin', 'HEAD:automation/release-v1.5.0-test'], cwd=source, check=False)
    assert previous.returncode != 0, previous.stdout + previous.stderr

    run(['git', 'push', 'origin', f'HEAD:{full_ref}'], cwd=source)
    run(['git', 'ls-remote', '--exit-code', 'origin', full_ref], cwd=source)

    run(['git', 'push', 'origin', f':{full_ref}'], cwd=source)
    missing = run(['git', 'ls-remote', '--exit-code', 'origin', full_ref], cwd=source, check=False)
    assert missing.returncode != 0, missing.stdout + missing.stderr

print('OK: detached HEAD temporary release ref requires and supports a fully qualified destination.')
