#!/usr/bin/env python3
"""Validate release gettext and offline manual payloads from canonical owners."""
from __future__ import annotations

import argparse
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def release_languages() -> list[str]:
    """Read the existing CMake registry; English has no gettext catalog."""
    registry = (ROOT / 'cmake/PerastageLocalization.cmake').read_text(encoding='utf-8')
    match = re.search(r'set\(PERASTAGE_TRANSLATION_LANGUAGES\s+([^)]*)\)', registry)
    if not match or not match.group(1).split():
        raise ValueError('Missing release locale registry')
    return match.group(1).split()


def manual_names() -> set[str]:
    """Use the canonical user index to discover the release page inventory."""
    index = (ROOT / 'docs/user/index.md').read_text(encoding='utf-8')
    return {'index.md'} | set(re.findall(r'\]\(([^/#)]+\.md)\)', index))


def verify(asset_root: Path, catalogs: bool = True) -> None:
    """Require complete, byte-identical manuals and compiled gettext assets."""
    languages = release_languages()
    names = manual_names()
    help_root = asset_root / 'help'
    expected = {'en', *languages}
    actual = {p.name for p in help_root.iterdir() if p.is_dir()}
    if actual != expected:
        raise ValueError(f'Help locales differ at {help_root}: {actual} != {expected}')
    for language in ['en', *languages]:
        pages = help_root / language
        if {p.name for p in pages.iterdir()} != names:
            raise ValueError(f'Manual inventory differs at {pages}')
        source = ROOT / 'docs/user'
        if language != 'en':
            source /= f'locales/{language}'
        for name in names:
            if (pages / name).read_bytes() != (source / name).read_bytes():
                raise ValueError(f'Manual content differs: {pages / name}')
    if catalogs:
        locale_root = asset_root / 'locale'
        if not locale_root.is_dir():
            locale_root = asset_root / 'resources/locale'
        for language in languages:
            catalog = locale_root / language / 'LC_MESSAGES/perastage.mo'
            if not catalog.is_file() or catalog.stat().st_size == 0:
                raise ValueError(f'Missing compiled catalog: {catalog}')
    if (asset_root / 'help.md').exists():
        raise ValueError('Legacy root help.md must not be packaged')


def verify_inventory(inventory: Path, prefix: str) -> None:
    """Check the final Arch archive listing without requiring extraction."""
    paths = {line.strip().removeprefix('./') for line in inventory.read_text().splitlines()}
    prefix = prefix.rstrip('/') + '/'
    languages = release_languages()
    expected_help = {f'{prefix}help/{lang}/{name}'
                     for lang in ['en', *languages] for name in manual_names()}
    actual_help = {p for p in paths if p.startswith(prefix + 'help/') and not p.endswith('/')}
    if actual_help != expected_help:
        raise ValueError('Archive manual inventory differs from canonical owners')
    for lang in languages:
        if f'{prefix}resources/locale/{lang}/LC_MESSAGES/perastage.mo' not in paths:
            raise ValueError(f'Missing archive catalog: {lang}')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--asset-root', type=Path)
    group.add_argument('--inventory', type=Path)
    parser.add_argument('--prefix', default='opt/perastage')
    parser.add_argument('--manual-only', action='store_true')
    args = parser.parse_args()
    if args.asset_root:
        verify(args.asset_root, catalogs=not args.manual_only)
    else:
        verify_inventory(args.inventory, args.prefix)
    print('OK: complete release localization payload matches the CMake registry and user index.')


if __name__ == '__main__':
    main()
