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
    """Require byte-identical Markdown payloads and retained transitional help."""
    actual = {p.relative_to(asset_root / 'help').as_posix(): p.read_bytes()
              for p in (asset_root / 'help').rglob('*') if p.is_file()}
    assert actual == expected, f"manual payload mismatch at {asset_root}: {actual.keys()}"
    assert (asset_root / 'help/en/index.md').is_file()
    assert (asset_root / 'help.md').read_bytes() == (ROOT / 'help.md').read_bytes()


def exercise(cmake: str, translated: bool) -> None:
    """Test absent or partial locales using the actual staging/install owners."""
    with tempfile.TemporaryDirectory(prefix='perastage-manual-') as tmp:
        source = Path(tmp) / 'source with spaces'
        build = Path(tmp) / 'build'
        source.mkdir()
        for name in ('cmake', 'packaging/linux'):
            shutil.copytree(ROOT / name, source / name)
        (source / 'licenses').mkdir()
        (source / 'resources').mkdir()
        (source / 'resources/Perastage_logo_1024.png').write_bytes(b'fixture')
        for name in ('LICENSE.txt', 'THIRD_PARTY_NOTICES.md', 'help.md'):
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
        if translated:
            registry = (ROOT / 'cmake/PerastageLocalization.cmake').read_text()
            languages = re.search(r'set\(PERASTAGE_TRANSLATION_LANGUAGES\s+([^)]+)\)', registry)
            assert languages
            for language in languages.group(1).split():
                locale = source / 'docs/user/locales' / language
                locale.mkdir()
                payload = f'fixture {language}\n'.encode()
                (locale / 'index.md').write_bytes(payload)
                (locale / 'not-manual.txt').write_text('excluded')
                expected[f'{language}/index.md'] = payload
        # Include the existing registry owner with UI localization disabled.
        (source / 'main.c').write_text('int main(void) { return 0; }\n')
        (source / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.21)
project(Perastage LANGUAGES C)
set(PERASTAGE_ENABLE_LOCALIZATION OFF)
include(cmake/PerastageLocalization.cmake)
add_executable(Perastage MACOSX_BUNDLE main.c)
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


def main() -> None:
    """Run both optional-locale scenarios with the configured CMake executable."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--cmake', default=shutil.which('cmake'))
    args = parser.parse_args()
    if not args.cmake:
        parser.error('CMake is required for staging validation')
    for translated in (False, True):
        exercise(args.cmake, translated)
    print('OK: runtime/install help/en, optional locales, Markdown isolation and help.md.')


if __name__ == '__main__':
    main()
