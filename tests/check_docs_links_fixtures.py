#!/usr/bin/env python3
"""Prove the shared documentation checker rejects broken complete manual mirrors."""
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    """Exercise parity, encoding, stable syntax and link failures in an isolated tree."""
    with tempfile.TemporaryDirectory(prefix='perastage-doc-links-') as tmp:
        root = Path(tmp)
        for directory in ('docs', 'cmake', '.github'):
            shutil.copytree(ROOT / directory, root / directory)
        for name in ('README.md', 'AGENTS.md'):
            shutil.copyfile(ROOT / name, root / name)
        (root / 'tests').mkdir()
        checker = root / 'tests/check_docs_links.py'
        shutil.copyfile(ROOT / 'tests/check_docs_links.py', checker)
        registry = (root / 'cmake/PerastageLocalization.cmake').read_text()
        match = re.search(r'set\(PERASTAGE_TRANSLATION_LANGUAGES\s+([^)]+)\)', registry)
        assert match
        pages = sorted((root / 'docs/user').glob('*.md'))
        assert pages

        def check(expected: str | None = None) -> None:
            result = subprocess.run([sys.executable, str(checker)], capture_output=True, text=True)
            assert (result.returncode == 0) == (expected is None), result.stdout + result.stderr
            if expected:
                assert expected in result.stderr, result.stderr

        check()
        for locale in match[1].split():
            mirror = root / 'docs/user/locales' / locale
            page = mirror / pages[0].name
            original = page.read_bytes()
            page.unlink()
            check('missing canonical page')
            page.write_bytes(original)
            extra = mirror / 'unexpected.md'
            extra.write_text('# Unexpected\n')
            check('unexpected page')
            extra.unlink()
            for payload, error in (
                    (b' \n', 'is empty'), (b'\xff', 'not UTF-8'),
                    (original + b'\n[Missing](nonexistent.md)\n', 'missing link'),
                    (original + b'\n[Bad fragment](preferences.md#nonexistent)\n', 'missing fragment'),
                    (original + b'\n[Local fragment](#nonexistent)\n', 'missing fragment'),
                    (original + b'\n[](preferences.md)\n', 'empty Markdown link'),
                    (original + b'\n[Empty]()\n', 'empty Markdown link'),
                    (original + b'\n[Developer](https://github.com/PeramatoG/Perastage/blob/main/docs/developer/nonexistent.md)\n', 'missing English owner')):
                page.write_bytes(payload)
                check(error)
                page.write_bytes(original)
            commands = mirror / 'shortcuts-and-command-bar.md'
            original_commands = commands.read_bytes()
            for payload, error in (
                    (original_commands.replace(b'<!-- PERASTAGE_CONSOLE_HELP_BEGIN -->', b''), 'Console help marker'),
                    (original_commands + b'\n<!-- PERASTAGE_CONSOLE_HELP_END -->', 'Console help marker'),
                    (original_commands.replace(b'`f 12`', b'`f 13`'), 'stable Console syntax')):
                commands.write_bytes(payload)
                check(error)
                commands.write_bytes(original_commands)
            install = mirror / 'installation.md'
            original_install = install.read_bytes()
            install.write_bytes(original_install.replace(b'/releases/latest', b'/releases/changed'))
            check('rewrites an external URL')
            install.write_bytes(original_install)
        check()
    print('OK: complete manual parity/link/fragment/encoding/Console guard failure fixtures.')


if __name__ == '__main__':
    main()
