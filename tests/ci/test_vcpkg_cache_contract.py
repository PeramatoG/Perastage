"""Focused contract tests without runner images, network access, or vcpkg builds."""
import importlib.util
import os
from pathlib import Path
import re
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]


def load_script(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / '.github/scripts' / f'{name}.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


CONTRACT = load_script('vcpkg_cache_contract')
SUMMARY = load_script('write_vcpkg_cache_summary')


def step(text, step_id):
    return next(block for block in re.split(r'(?=      - name: )', text)
                if f'\n        id: {step_id}\n' in block)


class CacheContractTest(unittest.TestCase):
    def key(self, **overrides):
        args = dict(runner_os='Linux', arch='X64', triplet='x64-linux', runtime='ubuntu-24.04',
                    tools={'gcc': '13.3.0', 'cmake': '3.31.6'}, baseline='baseline', manifest='manifest')
        args.update(overrides)
        return CONTRACT.keys(**args)

    def test_routine_image_updates_do_not_change_identity(self):
        with patch.dict(os.environ, {'ImageVersion': 'old'}):
            old = self.key()
        with patch.dict(os.environ, {'ImageVersion': 'new'}):
            self.assertEqual(old, self.key())
        self.assertEqual(self.key(tools={'cmake': '3.31.6', 'gcc': '13.3.0'}), old)

    def test_ubuntu_migration_is_a_runtime_boundary(self):
        old = self.key()
        new = self.key(runtime='ubuntu-26.04')
        self.assertNotEqual(old['runtime-fallback'], new['runtime-fallback'])
        self.assertFalse(new['prefix'].startswith(old['runtime-fallback']))

    def test_tool_changes_can_restore_abi_checked_archives(self):
        old = self.key()
        new = self.key(tools={'gcc': '14.2.0'})
        self.assertNotEqual(old['prefix'], new['prefix'])
        self.assertEqual(old['runtime-fallback'], new['runtime-fallback'])
        self.assertEqual(old['runtime-fallback'], self.key(baseline='new')['runtime-fallback'])
        self.assertNotEqual(old['prefix'], self.key(manifest='new')['prefix'])

    def test_sdk_boundary_is_required_and_preserved(self):
        with self.assertRaises(ValueError), patch.object(CONTRACT, 'probe', return_value='version'):
            CONTRACT.environment_identity('macOS', '')
        old = self.key(runner_os='macOS', runtime='sdk-resolved-xcode-and-path')
        new = self.key(runner_os='macOS', runtime='sdk-other')
        self.assertNotEqual(old['runtime-fallback'], new['runtime-fallback'])

    def test_snapshot_repairs_are_deterministic(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.assertEqual(CONTRACT.snapshot(root)[1], 0)
            (root / 'a.zip').write_bytes(b'old ABI archive')
            first = CONTRACT.snapshot(root)
            self.assertEqual(first, CONTRACT.snapshot(root))
            (root / 'b.zip').write_bytes(b'new ABI archive')
            repaired = CONTRACT.snapshot(root)
            self.assertNotEqual(first[0], repaired[0])
            self.assertEqual(repaired[1], 2)
            (root / 'b.zip').write_bytes(b'repaired content')
            self.assertNotEqual(repaired, CONTRACT.snapshot(root))
            (root / 'unrelated.log').write_text('ignored')
            self.assertEqual(CONTRACT.snapshot(root)[1], 2)

    def test_repair_can_supersede_a_newer_incomplete_snapshot(self):
        prefix = self.key()['prefix']
        old_complete = CONTRACT.snapshot_key(prefix, 'complete', '')
        newer_incomplete = CONTRACT.snapshot_key(prefix, 'incomplete', '')
        repaired = CONTRACT.snapshot_key(prefix, 'complete', newer_incomplete)
        self.assertNotEqual(repaired, old_complete)
        self.assertEqual(repaired, CONTRACT.snapshot_key(prefix, 'complete', newer_incomplete))
        self.assertEqual(repaired, CONTRACT.snapshot_key(prefix, 'complete', repaired))

    def test_producers_and_consumers_share_paths_and_key_contract(self):
        ci = (ROOT / '.github/workflows/ci-tests.yml').read_text()
        warm = (ROOT / '.github/workflows/vcpkg-binary-cache.yml').read_text()
        producer = step(warm, 'vcpkg-cache')
        expected = producer[producer.index('        with:'):]
        for job in ('linux-debug', 'windows-debug', 'macos-debug'):
            text = ci[ci.index(f'  {job}:'):]
            consumer = step(text, 'vcpkg-cache')
            self.assertEqual(consumer[consumer.index('        with:'):], expected)
            for cache_step in ('vcpkg-cache', 'vcpkg-cache-save'):
                block = step(text, cache_step)
                self.assertIn('path: .vcpkg-cache/binary', block)
                self.assertNotIn('.vcpkg-cache/installed', block)
                self.assertNotIn('.vcpkg-cache/packages', block)
            identity = step(text, 'vcpkg-contract')
            self.assertIn('vcpkg_cache_contract.py identity', identity)
            self.assertIn("hashFiles('vcpkg.json', 'vcpkg-configuration.json')", identity)
            if job == "macos-debug":
                self.assertIn('--sdk "${{ steps.macos-sdk.outputs.identity }}"', identity)
            save = step(text, 'vcpkg-cache-save')
            self.assertIn('steps.vcpkg-snapshot.outputs.key', save)
            self.assertNotIn("cache-hit != 'true'", save)
            self.assertIn("sccache_gha_enabled == 'on'", step(text, 'vcpkg-snapshot'))
        self.assertIn('path: .vcpkg-cache/binary', step(warm, 'vcpkg-cache-save'))
        self.assertIn('!inputs.bypass_local_compiled_cache', step(warm, 'vcpkg-snapshot'))
        self.assertIn('--mode readwrite', warm)
        self.assertEqual(ci.count('--mode read\n'), 2)  # Bash consumers; Windows uses a quoted scalar.
        self.assertIn('--mode read\'', ci)
        self.assertIn('runs-on: ubuntu-latest', ci)
        self.assertIn('runs-on: windows-latest', ci)

    def test_provider_totals_and_zero_rebuilds(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / 'install.log'
            log.write_text('Restored 40 package(s) from local in 2 s.\n'
                           'Restored 0 package(s) from NuGet in 120 s.\n'
                           'All requested installations completed successfully in: 3 min\n')
            self.assertEqual(SUMMARY.parse_install_activity(log), ('unknown', '40', '0'))
            report = '\n'.join(SUMMARY.remote_activity(log))
            self.assertIn("('0', '120 s')", report)
            self.assertIn('independent read-back still required', report)


if __name__ == '__main__':
    unittest.main()
