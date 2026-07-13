#!/usr/bin/env python3
"""Maintains Perastage gettext templates, catalogs, and source-level localization checks."""
from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
POT = ROOT / "resources" / "locale" / "perastage.pot"
LANGUAGES = ["es"]
SOURCE_DIRS = ["core", "gui", "viewer2d", "viewer3d", "viewer_common", "mvr"]
SOURCE_SUFFIXES = {".cpp", ".cxx", ".cc", ".h", ".hpp"}
AUDIT_ALLOWLIST = ROOT / "scripts" / "localization_audit_allowlist.txt"

UI_PATTERNS = [
    re.compile(r"\bwxMessageBox\s*\([^\n]*\""),
    re.compile(r"\bwxBusyInfo\s*\([^\n]*\""),
    re.compile(r"\bSetStatusText\s*\([^\n]*\""),
    re.compile(r"\bSplashScreen::SetMessage\s*\([^\n]*\""),
    re.compile(r"\bAppendColumn\s*\([^\n]*\""),
    re.compile(r"\bAppendCheckItem\s*\([^\n]*\""),
    re.compile(r"\bAppendSubMenu\s*\([^\n]*\""),
    re.compile(r"\bSetToolTip\s*\([^\n]*\""),
    re.compile(r"\bAppendMessage\s*\([^\n]*\""),
    re.compile(r"\bSetLabel\s*\([^\n]*\""),
    re.compile(r"\bSetTitle\s*\([^\n]*\""),
    re.compile(r"\bAppend\s*\([^\n]*\""),
    re.compile(r"\bnew\s+wx(?:StaticText|Button|CheckBox|RadioButton|Choice|Dialog|Frame|StaticBox)\s*\([^\n]*\""),
    re.compile(r"\bwx(?:FileDialog|DirDialog|TextEntryDialog|SingleChoiceDialog|ProgressDialog)\s*\([^\n]*\""),
]
WRAPPED_MARKERS = ("_(\"", "wxGetTranslation(\"", "wxGetTranslation(wxString", "wxPLURAL(")


def run(cmd: list[str]) -> None:
    subprocess.run(cmd, cwd=ROOT, check=True)


def source_files() -> list[Path]:
    try:
        result = subprocess.run(
            ["git", "ls-files"], cwd=ROOT, check=True, text=True, capture_output=True
        )
        paths = [ROOT / line for line in result.stdout.splitlines()]
    except Exception:
        paths = [p for directory in SOURCE_DIRS for p in (ROOT / directory).rglob("*")]
    files: list[Path] = []
    for path in paths:
        if not path.is_file() or path.suffix not in SOURCE_SUFFIXES:
            continue
        rel = path.relative_to(ROOT).as_posix()
        if not any(rel.startswith(f"{directory}/") for directory in SOURCE_DIRS):
            continue
        files.append(path)
    return sorted(files)


def update_pot() -> None:
    files = [str(path.relative_to(ROOT)) for path in source_files()]
    POT.parent.mkdir(parents=True, exist_ok=True)
    run([
        "xgettext",
        "--language=C++",
        "--from-code=UTF-8",
        "--keyword=_",
        "--keyword=wxGetTranslation",
        "--keyword=wxPLURAL:1,2",
        "--add-comments=TRANSLATORS:",
        "--package-name=Perastage",
        "--package-version=1.5.0",
        "--output",
        str(POT.relative_to(ROOT)),
        *files,
    ])


def po_path(language: str) -> Path:
    return ROOT / "resources" / "locale" / language / "LC_MESSAGES" / "perastage.po"


def update_po() -> None:
    update_pot()
    for language in LANGUAGES:
        path = po_path(language)
        run(["msgmerge", "--update", "--backup=none", str(path), str(POT)])


def iter_po_entries(path: Path):
    entry: list[str] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("msgid ") and entry:
            yield entry
            entry = [line]
        else:
            entry.append(line)
    if entry:
        yield entry


def check_po() -> int:
    failures: list[str] = []
    for language in LANGUAGES:
        path = po_path(language)
        run(["msgfmt", "--check", "-o", os.devnull, str(path)])
        for entry in iter_po_entries(path):
            if any(line.startswith("#~") for line in entry):
                continue
            msgid_lines = [line for line in entry if line.startswith("msgid ")]
            if msgid_lines and msgid_lines[0] == 'msgid ""':
                continue
            if any("#," in line and "fuzzy" in line for line in entry):
                failures.append(f"{path}: fuzzy translation for {msgid_lines[0] if msgid_lines else '<unknown>'}")
            msgstr_lines = [line for line in entry if line.startswith("msgstr")]
            if any(line.endswith('""') for line in msgstr_lines):
                failures.append(f"{path}: empty translation for {msgid_lines[0] if msgid_lines else '<unknown>'}")
    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    return 0


def load_allowlist() -> set[str]:
    if not AUDIT_ALLOWLIST.exists():
        return set()
    return {
        line.strip()
        for line in AUDIT_ALLOWLIST.read_text(encoding="utf-8").splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    }


def audit() -> int:
    allowlist = load_allowlist()
    findings: list[str] = []
    for path in source_files():
        rel = path.relative_to(ROOT).as_posix()
        for number, line in enumerate(path.read_text(encoding="utf-8", errors="ignore").splitlines(), start=1):
            if any(marker in line for marker in WRAPPED_MARKERS):
                continue
            if any(pattern.search(line) for pattern in UI_PATTERNS):
                key = f"{rel}:{number}"
                if key not in allowlist:
                    findings.append(f"{key}: {line.strip()}")
    if findings:
        print("High-confidence unmarked UI strings found:", file=sys.stderr)
        print("\n".join(findings), file=sys.stderr)
        print("Add gettext marking or document a stable technical exception in scripts/localization_audit_allowlist.txt.", file=sys.stderr)
        return 1
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["update-pot", "update-po", "check-po", "audit"])
    args = parser.parse_args()
    if args.action == "update-pot":
        update_pot(); return 0
    if args.action == "update-po":
        update_po(); return 0
    if args.action == "check-po":
        return check_po()
    if args.action == "audit":
        return audit()
    return 1

if __name__ == "__main__":
    raise SystemExit(main())
