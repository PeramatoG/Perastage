#!/usr/bin/env python3
"""Prove the shared documentation checker rejects broken complete manual mirrors."""
from pathlib import Path
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def check_windows_checkout(root: Path) -> None:
    """Exercise real Git autocrlf conversion while retaining exact artifact bytes."""
    source = root / 'checkout-source'
    checkout = root / 'checkout-result'
    source.mkdir()
    checkout.mkdir()
    shutil.copyfile(root / '.gitattributes', source / '.gitattributes')
    expected = {}
    for file in [*(root / 'docs').rglob('*.html'), *(root / 'docs/stable').rglob('*')]:
        if not file.is_file():
            continue
        name = file.relative_to(root).as_posix()
        expected[name] = file.read_bytes()
    # Release-specific text resources may legitimately contain either line-ending style.
    expected['docs/stable/content/tagged-lf.txt'] = b'Tagged content\nKeep these bytes\n'
    expected['docs/stable/content/tagged-crlf.txt'] = b'Tagged content\r\nKeep these bytes\r\n'
    for name, content in expected.items():
        file = source / name
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_bytes(content)
    (source / 'unmanaged.txt').write_bytes(b'Ordinary text\n')

    def git(*arguments: str) -> None:
        """Use a fixture index and explicit Windows checkout settings on every host."""
        subprocess.run(['git', '-c', 'core.autocrlf=true', '-c', 'core.eol=crlf',
                        '-c', 'core.safecrlf=false', *arguments], cwd=source,
                       check=True, capture_output=True)

    git('init', '--quiet', '--template=')
    git('add', '--', '.gitattributes', 'docs', 'unmanaged.txt')
    git('checkout-index', '--all', '--prefix=' + checkout.as_posix() + '/')
    assert (checkout / 'unmanaged.txt').read_bytes() == b'Ordinary text\r\n', 'Windows conversion must be active in the fixture'
    manifest = json.loads((root / 'docs/stable/manifest.json').read_text(encoding='utf-8'))
    for name, digest in manifest['artifacts'].items():
        assert hashlib.sha256((checkout / 'docs/stable' / name).read_bytes()).hexdigest() == digest, (
            f'Windows autocrlf checkout changes Stable artifact bytes: {name}')
    for name, content in expected.items():
        assert (checkout / name).read_bytes() == content, f'Windows autocrlf checkout changes generated/resource bytes: {name}'


def main() -> None:
    """Exercise parity, encoding, stable syntax and link failures in an isolated tree."""
    with tempfile.TemporaryDirectory(prefix='perastage-doc-links-') as tmp:
        root = Path(tmp)
        for directory in ('docs', 'cmake', '.github'):
            shutil.copytree(ROOT / directory, root / directory)
        for name in ('README.md', 'AGENTS.md', '.gitattributes'):
            shutil.copyfile(ROOT / name, root / name)
        (root / 'tests').mkdir()
        checker = root / 'tests/check_docs_links.py'
        shutil.copyfile(ROOT / 'tests/check_docs_links.py', checker)
        shutil.copyfile(ROOT / 'tests/docs_static_validation.py', root / 'tests/docs_static_validation.py')
        registry = (root / 'cmake/PerastageLocalization.cmake').read_text(encoding='utf-8')
        match = re.search(r'set\(PERASTAGE_TRANSLATION_LANGUAGES\s+([^)]+)\)', registry)
        assert match
        pages = sorted((root / 'docs/user').glob('*.md'))
        assert pages

        def check(expected: str | None = None) -> None:
            result = subprocess.run([sys.executable, str(checker)], capture_output=True, text=True)
            assert (result.returncode == 0) == (expected is None), f'Expected {expected}: ' + result.stdout + result.stderr
            if expected:
                assert expected in result.stderr, result.stderr

        check()
        check_windows_checkout(root)
        # A checkout policy must not weaken the manifest's exact-byte integrity guard.
        stable_page = root / 'docs/stable/quick-start.html'
        original_stable = stable_page.read_bytes()
        for payload in (original_stable + b'<!-- tampered -->\n',
                        original_stable.replace(b'\n', b'\r\n')):
            stable_page.write_bytes(payload)
            check('Stable hash mismatch: quick-start.html')
            stable_page.write_bytes(original_stable)
        # Mutate generated sidebar metadata while keeping the canonical owner intact.
        page = root / 'docs/next/installation.html'
        original_page = page.read_text(encoding='utf-8')
        install_entry = '<li><a class="active" aria-current="page" href="installation.html">Installation</a></li>'
        quick_entry = '<li><a href="quick-start.html">Quick Start</a></li>'
        swapped = original_page.replace(install_entry, 'ENTRY_PLACEHOLDER').replace(
            quick_entry, install_entry).replace('ENTRY_PLACEHOLDER', quick_entry)
        for payload, error in (
                (original_page.replace('>Getting Started</h2>', '>Setup</h2>'), 'mirror canonical index'),
                (original_page.replace('>Installation</a>', '>Install</a>'), 'mirror canonical index'),
                (original_page.replace(install_entry, ''), 'mirror canonical index'),
                (original_page.replace(install_entry, install_entry * 2), 'mirror canonical index'),
                (swapped, 'mirror canonical index'),
                (original_page.replace('href="installation.html"', 'href="install.html"'), 'mirror canonical index'),
                (original_page.replace('href="installation.html"', 'href="developer/build.html"'), 'mirror canonical index'),
                (original_page.replace('>Help</h2>', '>Reference</h2>'), 'mirror canonical index'),
                (re.sub(r'<h1\b[^>]*>', '<p>', original_page), 'lacks static content'),
                (re.sub(r'<meta name="description"[^>]*>', '', original_page), 'missing metadata: description'),
                (original_page.replace('content="noindex,follow"', 'content="index,follow"'), 'Next must remain noindex'),
                (original_page.replace('href="https://perastage.luismaperamato.com/next/installation.html"', 'href="installation.html"'), 'canonical must be'),
                (original_page.replace('<div id="content" tabindex="-1">', '<div id="content" tabindex="-1"><h1>Duplicate</h1>'), 'exactly one primary h1'),
                (original_page.replace('Generated from canonical Markdown.', 'Hand edited.'), 'missing generated marker'),
                (original_page.replace('assets/js/docs-nav.js', 'assets/js/marked.min.js'), 'runtime Markdown'),
                (original_page.replace('href="preferences.html"', 'href="preferences.html#nonexistent"'), 'missing generated fragment'),
                (original_page.replace('assets/images/logo-web.png', 'assets/images/missing.png'), 'missing generated link/resource')):
            page.write_text(payload, encoding='utf-8')
            check(error)
            page.write_text(original_page, encoding='utf-8')
        index = root / 'docs/user/index.md'
        original_index = index.read_text(encoding='utf-8')
        index.write_text(original_index.replace('- [Installation](installation.md)', ''), encoding='utf-8')
        check('index must list every user page exactly once')
        index.write_text(original_index, encoding='utf-8')
        wrapper = root / 'docs/installation.html'
        original_wrapper = wrapper.read_bytes()
        wrapper.unlink()
        check('public user route is missing')
        wrapper.write_bytes(original_wrapper)
        landing = root / 'docs/next/documentation.html'
        original_landing = landing.read_text(encoding='utf-8')
        landing.write_text(original_landing.replace('href="../developer/index.html"', 'href="build.html"'), encoding='utf-8')
        check('missing required marker')
        landing.write_text(original_landing, encoding='utf-8')
        for locale in match[1].split():
            mirror = root / 'docs/user/locales' / locale
            page = mirror / pages[0].name
            original = page.read_bytes()
            page.unlink()
            check('missing canonical page')
            page.write_bytes(original)
            extra = mirror / 'unexpected.md'
            extra.write_text('# Unexpected\n', encoding='utf-8')
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
    print('OK: Windows autocrlf checkout byte preservation, exact Stable hashes, canonical navigation/routes and complete manual parity/link/fragment/encoding/Console guard failure fixtures.')


if __name__ == '__main__':
    main()
