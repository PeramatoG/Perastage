#!/usr/bin/env python3
"""Maintains Perastage gettext templates, catalogs, and source-level localization checks."""
from __future__ import annotations

import argparse
import ast
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
POT = ROOT / "resources" / "locale" / "perastage.pot"
LANGUAGES = ["es"]
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
EXCLUDED_PREFIXES = ("third_party/", "build/", "build-", "cmake-build", ".git/")
AUDIT_ALLOWLIST = ROOT / "scripts" / "localization_audit_allowlist.txt"
AUDIT_SOURCE_PREFIXES = ("gui/fixturetablepanel.cpp", "gui/trusstablepanel.cpp", "gui/hoisttablepanel.cpp", "gui/sceneobjecttablepanel.cpp", "gui/riggingpanel.cpp", "gui/layerpanel.cpp", "gui/summarypanel.cpp", "gui/addtrussdialog.cpp", "gui/gdtfsearchdialog.cpp", "gui/scene_object_primitive_dialogs.cpp", "gui/mainwindow.cpp", "gui/mainwindow_print.cpp", "gui/consolepanel.cpp")

TRANSLATION_WRAPPERS = {"_", "wxGetTranslation", "wxTRANSLATE", "wxPLURAL"}
UI_CALLEES = {
    "wxMessageBox", "wxBusyInfo", "wxProgressDialog", "SplashScreen::SetMessage",
    "SetStatusText", "SetHighlightedStatus", "SetLabel", "SetTitle",
    "SetToolTip", "SetHint", "Append", "AppendSubMenu", "AppendCheckItem",
    "AppendTextColumn", "AppendToggleColumn", "AddPage", "Caption", "AddTool",
    "wxFileDialog", "wxDirDialog", "wxTextEntryDialog", "wxSingleChoiceDialog",
    "wxStaticText", "wxButton", "wxCheckBox", "wxRadioButton", "wxStaticBox",
    "wxStaticBoxSizer", "wxDataViewColumn", "AppendColumn", "AppendItem", "Units::LabelWithUnit",
    "UpdatePaneCaption", "GetTextExtent", "BuildTooltip", "BuildHelp",
}
REPRESENTATIVE_MESSAGES = {
    "Running library bootstrap...": "root main.cpp splash message",
    "Fixture ID": "fixture table dynamic column label",
    "Model file": "fixture table dynamic column label",
    "Color Filter": "fixture table dynamic column label",
    "Weight (kg)": "fixture table translated runtime column label",
    "Hoist ID": "hoist table column label",
    "Chain Length": "hoist table column label",
    "Visible": "layer and summary visibility label",
    "Count": "summary count label",
    "Position": "rigging table column label",
    "Fixture Weight": "rigging dynamic weight label",
    "Console commands": "console help title",
    "Create scene from text": "rider text dialog title",
    "Search GDTF": "GDTF search dialog title",
    "Manufacturer:": "GDTF and metadata label",
    "Download": "download action label",
    "Enter new layer name:": "layer prompt label",
    "Add Cube": "scene-object primitive dialog title",
    "Select what to print:": "print choice dialog prompt",
    "Do you want to save changes before %s?": "exit/save confirmation",
    "Show available console commands and examples.": "console help tooltip",
    "Online GDTF catalog refresh failed.\n%s": "GDTF refresh warning",
}

class Tools:
    def __init__(self, args: argparse.Namespace):
        self.xgettext = args.xgettext or os.environ.get("PERASTAGE_XGETTEXT") or "xgettext"
        self.msgmerge = args.msgmerge or os.environ.get("PERASTAGE_MSGMERGE") or "msgmerge"
        self.msgfmt = args.msgfmt or os.environ.get("PERASTAGE_MSGFMT") or "msgfmt"
        self.msgattrib = args.msgattrib or os.environ.get("PERASTAGE_MSGATTRIB") or "msgattrib"


def run(cmd: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=ROOT, check=True, text=True, capture_output=True)


def repository_version() -> str:
    version_file = ROOT / "VERSION"
    return version_file.read_text(encoding="utf-8").strip() if version_file.exists() else "unknown"


def source_files() -> list[Path]:
    result = run(["git", "ls-files"])
    files: list[Path] = []
    for item in result.stdout.splitlines():
        if item.startswith(EXCLUDED_PREFIXES):
            continue
        path = ROOT / item
        if path.is_file() and path.suffix in SOURCE_SUFFIXES:
            files.append(path)
    return sorted(files)


def xgettext_command(output: Path, tools: Tools) -> list[str]:
    files = [str(path.relative_to(ROOT)) for path in source_files()]
    return [
        tools.xgettext,
        "--language=C++",
        "--from-code=UTF-8",
        "--keyword=_",
        "--keyword=wxGetTranslation",
        "--keyword=wxTRANSLATE",
        "--keyword=wxPLURAL:1,2",
        "--add-comments=TRANSLATORS:",
        "--package-name=Perastage",
        f"--package-version={repository_version()}",
        "--output",
        str(output.relative_to(ROOT) if output.is_relative_to(ROOT) else output),
        *files,
    ]


def update_pot(tools: Tools, output: Path = POT) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    run(xgettext_command(output, tools))


def po_path(language: str) -> Path:
    return ROOT / "resources" / "locale" / language / "LC_MESSAGES" / "perastage.po"


def update_po(tools: Tools) -> None:
    update_pot(tools)
    for language in LANGUAGES:
        run([tools.msgmerge, "--update", "--backup=none", str(po_path(language)), str(POT)])


def po_messages(path: Path) -> set[str]:
    result: set[str] = set()
    current: list[str] = []
    in_msgid = False
    obsolete = False
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw[3:] if raw.startswith("#~ ") else raw
        if raw.startswith("#~ "):
            obsolete = True
        if line.startswith("msgid "):
            if current and not obsolete:
                result.add("".join(current))
            obsolete = raw.startswith("#~ ")
            current = [ast.literal_eval(line[6:].strip())]
            in_msgid = True
        elif in_msgid and line.startswith('"'):
            current.append(ast.literal_eval(line.strip()))
        elif line.startswith("msgstr") or line.startswith("msgid_plural"):
            in_msgid = False
    if current and not obsolete:
        result.add("".join(current))
    result.discard("")
    return result


def check_catalog_sync(tools: Tools) -> int:
    with tempfile.TemporaryDirectory() as tempdir:
        temp_pot = Path(tempdir) / "perastage.pot"
        update_pot(tools, temp_pot)
        generated = po_messages(temp_pot)
    committed = po_messages(POT)
    failures: list[str] = []
    if generated != committed:
        missing = sorted(generated - committed)
        unexpected = sorted(committed - generated)
        failures.extend([f"POT is missing active message: {message}" for message in missing])
        failures.extend([f"POT has stale active message: {message}" for message in unexpected])
    for language in LANGUAGES:
        messages = po_messages(po_path(language))
        for message in sorted(committed - messages):
            failures.append(f"{language} PO is missing active POT message: {message}")
    for message, description in REPRESENTATIVE_MESSAGES.items():
        if message not in committed:
            failures.append(f"Representative {description} is missing from POT: {message}")
    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    return 0


def gettext_output_has_entries(command: list[str]) -> bool:
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True, check=True)
    return any(line.startswith("msgid ") and line != 'msgid ""' for line in result.stdout.splitlines())


def check_po(tools: Tools) -> int:
    failures: list[str] = []
    for language in LANGUAGES:
        path = po_path(language)
        subprocess.run([tools.msgfmt, "--check", "--check-accelerators=&", "-o", os.devnull, str(path)], cwd=ROOT, check=True)
        if gettext_output_has_entries([tools.msgattrib, "--only-fuzzy", str(path)]):
            failures.append(f"{path}: active fuzzy translations remain")
        if gettext_output_has_entries([tools.msgattrib, "--untranslated", str(path)]):
            failures.append(f"{path}: active untranslated entries remain")
    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    return check_catalog_sync(tools)


def parse_allowlist() -> dict[tuple[str, str], str]:
    exceptions: dict[tuple[str, str], str] = {}
    if not AUDIT_ALLOWLIST.exists():
        return exceptions
    for number, line in enumerate(AUDIT_ALLOWLIST.read_text(encoding="utf-8").splitlines(), start=1):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        parts = line.split("|", 3)
        if len(parts) != 4 or not all(part.strip() for part in parts):
            raise ValueError(f"{AUDIT_ALLOWLIST}:{number}: expected path|literal|category|reason")
        path, literal, category, reason = [part.strip() for part in parts]
        if category == "localization-debt":
            raise ValueError(f"{AUDIT_ALLOWLIST}:{number}: localization debt is not a valid exception")
        exceptions[(path, literal)] = f"{category}: {reason}"
    return exceptions


def find_matching_call(text: str, start: int) -> tuple[int, str]:
    open_index = text.find("(", start)
    if open_index < 0:
        return start, ""
    depth = 0
    in_string = False
    escaped = False
    for index in range(open_index, len(text)):
        char = text[index]
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            continue
        if char == '"':
            in_string = True
        elif char == '(':
            depth += 1
        elif char == ')':
            depth -= 1
            if depth == 0:
                return index + 1, text[start:index + 1]
    return len(text), text[start:]


def line_number(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def translated_spans(expression: str) -> list[tuple[int, int]]:
    spans: list[tuple[int, int]] = []
    wrapper_pattern = re.compile(r"\b(" + "|".join(re.escape(wrapper) for wrapper in TRANSLATION_WRAPPERS) + r")\s*\(")
    for match in wrapper_pattern.finditer(expression):
        end, _ = find_matching_call(expression, match.start())
        if end > match.start():
            spans.append((match.start(), end))
    return spans


def string_literals(expression: str) -> list[tuple[str, int]]:
    literals: list[tuple[str, int]] = []
    wrapped = translated_spans(expression)
    for match in re.finditer(r'"(?:\\.|[^"\\])*"', expression):
        if any(start <= match.start() < end for start, end in wrapped):
            continue
        try:
            value = ast.literal_eval(match.group(0))
        except Exception:
            value = match.group(0).strip('"')
        literals.append((value, match.start()))
    return literals


def audit() -> int:
    try:
        allowlist = parse_allowlist()
    except ValueError as error:
        print(error, file=sys.stderr)
        return 1
    findings: list[str] = []
    callee_pattern = re.compile(r"(?<![A-Za-z0-9_:])(" + "|".join(re.escape(callee) for callee in sorted(UI_CALLEES, key=len, reverse=True)) + r")\s*\(")
    for path in source_files():
        rel = path.relative_to(ROOT).as_posix()
        if not rel.startswith(AUDIT_SOURCE_PREFIXES):
            continue
        text = path.read_text(encoding="utf-8", errors="ignore")
        for match in callee_pattern.finditer(text):
            _, expression = find_matching_call(text, match.start())
            if not expression:
                continue
            for literal, local_offset in string_literals(expression):
                if not literal or not literal.strip() or literal in {"", "?", "[CMD] "}:
                    continue
                if (rel, literal) in allowlist:
                    continue
                findings.append(
                    f"{rel}:{line_number(text, match.start() + local_offset)}: unmarked UI literal {literal!r} in {expression.strip()}"
                )
    if findings:
        print("High-confidence unmarked UI strings found:", file=sys.stderr)
        print("\n".join(findings), file=sys.stderr)
        return 1
    return 0


def self_test() -> int:
    translated = 'wxMessageBox(_("Visible message"), _("Title"));'
    untranslated = 'wxMessageBox(\n  "Visible message",\n  _("Title"));'
    mixed = 'wxMessageBox(_("Title"), "Visible message");'
    static_marker = 'const char *x = wxTRANSLATE("Fixture ID");'
    assert not string_literals(translated)
    assert string_literals(untranslated)[0][0] == "Visible message"
    assert string_literals(mixed)[0][0] == "Visible message"
    unit_literal = 'Units::LabelWithUnit("Weight", suffix);'
    assert not string_literals(static_marker)
    assert string_literals(unit_literal)[0][0] == "Weight"
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["update-pot", "update-po", "check-po", "audit", "self-test", "count-sources"])
    parser.add_argument("--msgfmt")
    parser.add_argument("--xgettext")
    parser.add_argument("--msgmerge")
    parser.add_argument("--msgattrib")
    args = parser.parse_args()
    tools = Tools(args)
    if args.action == "update-pot":
        update_pot(tools); return 0
    if args.action == "update-po":
        update_po(tools); return 0
    if args.action == "check-po":
        return check_po(tools)
    if args.action == "audit":
        return audit()
    if args.action == "self-test":
        return self_test()
    if args.action == "count-sources":
        print(len(source_files())); return 0
    return 1

if __name__ == "__main__":
    raise SystemExit(main())
