#!/usr/bin/env python3
"""Exercise production CMake manual staging and installation with a tiny executable."""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(*args: str) -> None:
    """Run fixture tools and retain diagnostics only on failure."""
    result = subprocess.run(args, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(result.stdout)


def check_bundle(asset_root: Path, expected: dict[str, bytes]) -> None:
    """Require byte-identical Markdown payloads and exclude legacy root help."""
    actual = {p.relative_to(asset_root / 'help').as_posix(): p.read_bytes()
              for p in (asset_root / 'help').rglob('*') if p.is_file()}
    assert actual == expected, f"manual payload mismatch at {asset_root}: {actual.keys()}"
    assert (asset_root / 'help/en/index.md').is_file()
    assert (asset_root / 'help/en/shortcuts-and-command-bar.md').is_file()
    assert not (asset_root / 'help.md').exists()


def exercise(cmake: str, scenario: str) -> None:
    """Test complete, absent or partial locales using the actual staging/install owners."""
    with tempfile.TemporaryDirectory(prefix='perastage-manual-') as tmp:
        source = Path(tmp) / 'source with spaces'
        build = Path(tmp) / 'build'
        source.mkdir()
        for name in ('cmake', 'packaging/linux'):
            shutil.copytree(ROOT / name, source / name)
        (source / 'licenses').mkdir()
        (source / 'resources').mkdir()
        (source / 'resources/Perastage_logo_1024.png').write_bytes(b'fixture')
        for name in ('LICENSE.txt', 'THIRD_PARTY_NOTICES.md'):
            shutil.copyfile(ROOT / name, source / name)
        (source / 'dummy.gdtf').write_bytes(b'fixture')
        (source / 'docs/user').mkdir(parents=True)
        expected = {}
        for page in (ROOT / 'docs/user').glob('*.md'):
            shutil.copyfile(page, source / 'docs/user' / page.name)
            expected[f'en/{page.name}'] = page.read_bytes()
        # Unregistered and nested sources must never leak into English or other locales.
        (source / 'docs/user/locales/unregistered').mkdir(parents=True)
        (source / 'docs/user/locales/unregistered/index.md').write_text('excluded')
        if scenario != 'absent':
            registry = (ROOT / 'cmake/PerastageLocalization.cmake').read_text()
            languages = re.search(r'set\(PERASTAGE_TRANSLATION_LANGUAGES\s+([^)]+)\)', registry)
            assert languages
            for language in languages.group(1).split():
                locale = source / 'docs/user/locales' / language
                locale.mkdir()
                mirror = ROOT / 'docs/user/locales' / language
                canonical_names = {p.name for p in (ROOT / 'docs/user').glob('*.md')}
                assert {p.name for p in mirror.glob('*.md')} == canonical_names
                pages = sorted(mirror.glob('*.md')) if scenario == 'complete' else [mirror / 'index.md']
                for page in pages:
                    shutil.copyfile(page, locale / page.name)
                    expected[f'{language}/{page.name}'] = page.read_bytes()
                (locale / 'not-manual.txt').write_text('excluded')
                (locale / 'nested').mkdir()
                (locale / 'nested/index.md').write_text('excluded')
        # Include the existing registry owner with UI localization disabled.
        (source / 'main.c').write_text('int main(void) { return 0; }\n')
        (source / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.21)
project(Perastage LANGUAGES C)
set(PERASTAGE_ENABLE_LOCALIZATION OFF)
include(cmake/PerastageLocalization.cmake)
add_executable(Perastage MACOSX_BUNDLE main.c)
add_executable(perastage_cli main.c)
set_target_properties(perastage_cli PROPERTIES OUTPUT_NAME "perastage-cli")
set(PERASTAGE_GENERATED_DUMMY_GDTF_ARCHIVE "${CMAKE_SOURCE_DIR}/dummy.gdtf")
include(cmake/PerastageRuntimeStaging.cmake)
include(cmake/PerastageInstall.cmake)
''')
        run(cmake, '-S', str(source), '-B', str(build), '-DCMAKE_BUILD_TYPE=Release')
        run(cmake, '--build', str(build), '--config', 'Release', '--target', 'perastage_stage')
        runtime = build / 'Release' if (build / 'Release/help').is_dir() else build
        installed = source / 'out/install/Release'
        if sys.platform == 'darwin':
            runtime = (build / 'Release' if (build / 'Release').is_dir() else build)
            runtime /= 'Perastage.app/Contents/Resources'
            installed /= 'Perastage.app/Contents/Resources'
        check_bundle(runtime, expected)
        check_bundle(installed, expected)
        if scenario == 'complete':
            validator = str(ROOT / 'packaging/verify_localization.py')
            for payload in (runtime, installed):
                run(sys.executable, validator, '--manual-only', '--asset-root', str(payload))
            # Complete release validation must reject missing and unexpected pages.
            french_page = installed / 'help/fr/quick-start.md'
            content = french_page.read_bytes()
            french_page.unlink()
            result = subprocess.run([sys.executable, validator, '--manual-only',
                                     '--asset-root', str(installed)], capture_output=True)
            assert result.returncode != 0, 'missing release page was accepted'
            french_page.write_bytes(content)
            extra = installed / 'help/de/unregistered.md'
            extra.write_text('not canonical')
            result = subprocess.run([sys.executable, validator, '--manual-only',
                                     '--asset-root', str(installed)], capture_output=True)
            assert result.returncode != 0, 'extra release page was accepted'
            extra.unlink()
        executable_root = source / 'out/install/Release'
        if sys.platform == 'darwin':
            executable_root /= 'Perastage.app/Contents/MacOS'
        suffix = '.exe' if sys.platform == 'win32' else ''
        for name in ('Perastage', 'perastage-cli'):
            executable = executable_root / (name + suffix)
            assert executable.is_file(), f"stage must build and install {executable}"
        assert not list((source / 'out/install/Release').rglob('*mcp*'))


def main() -> None:
    """Run complete and resilience scenarios with the configured CMake executable."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--cmake', default=shutil.which('cmake'))
    args = parser.parse_args()
    if not args.cmake:
        parser.error('CMake is required for staging validation')
    for scenario in ('complete', 'absent', 'partial'):
        exercise(args.cmake, scenario)
    print('OK: byte-identical runtime/install manuals for every registered release locale; absent/partial locales, Markdown isolation and absence of root legacy help.')


if __name__ == '__main__':
    main()
