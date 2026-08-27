#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON_BIN="${PERASTAGE_TEST_PYTHON:-python3}"

"$PYTHON_BIN" - "$ROOT_DIR" <<'PY'
from pathlib import Path
import re
import sys

root = Path(sys.argv[1])

def read(relative: str) -> str:
    return (root / relative).read_text(encoding="utf-8")

def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"ERROR: {message}")

translation_call = re.compile(r"\b(?:_|wxGetTranslation|wxTRANSLATE|wxPLURAL)\s*\(")

# Core resolution metadata stays structured and locale-independent.
for relative in ("core/rider_fixture_resolution.cpp", "core/rider_fixture_resolution.h", "core/gdtfnet.cpp", "core/gdtfnet.h"):
    require(not translation_call.search(read(relative)), f"gettext crossed into {relative}")

# Console command grammar, help, output, and diagnostic prefixes remain stable.
console = read("gui/consolepanel.cpp")
process_start = console.index("void ConsolePanel::ProcessCommand")
require(not translation_call.search(console[process_start:]),
        "Console processor output must not depend on the UI locale")
for token in ("[INFO]", "[ERROR]", "[CMD]", "--group", "--local"):
    require(token in console, f"Console contract token disappeared: {token}")

# Primitive tokens, default names, and preference keys stay stable strings.
primitive_creation = read("gui/scene_object_primitive_creation.cpp")
primitive_dialogs = read("gui/scene_object_primitive_dialogs.cpp")
for token in ('"primitive:sphere"', '"primitive:cube"', '"primitive:cylinder"'):
    require(token in primitive_creation, f"serialized primitive token changed: {token}")
for token in ('"primitive_sphere_default_name"', '"primitive_cube_default_name"',
              '"primitive_cylinder_default_name"'):
    require(token in primitive_dialogs, f"primitive config key changed: {token}")
require('kUiLanguageConfigKey = "ui_language"' in read("core/localization/app_language.h"),
        "UI language configuration key changed")

# Localized choice labels dispatch by index and map to stable enum values.
fixture_replace = read("gui/mainwindow_fixture_replace.cpp")
require("const int sourceSelection = sourceDlg.GetSelection();" in fixture_replace and
        "sourceSelection == 0" in fixture_replace and "sourceSelection == 1" in fixture_replace,
        "fixture replacement choices no longer dispatch by stable index")
dictionary = read("gui/dictionaryeditdialog.cpp")
for policy in ("DictionaryImportPolicy::AddMissing", "DictionaryImportPolicy::AddAndOverwrite",
               "DictionaryImportPolicy::ReplaceAll"):
    require(policy in dictionary, f"dictionary policy mapping disappeared: {policy}")

# Undo history, diagnostics, schema tokens, and GDTF values remain non-localized contracts.
for path in root.glob("gui/**/*.cpp"):
    text = path.read_text(encoding="utf-8", errors="ignore")
    require(not re.search(r"PushUndoState\s*\(\s*(?:_|wxGetTranslation)\s*\(", text),
            f"localized Undo/Redo identity in {path.relative_to(root)}")
for relative in ("mvr/mvrexporter.cpp", "core/gdtfnet.cpp"):
    require(not translation_call.search(read(relative)),
            f"gettext entered serialization/protocol source {relative}")

print("OK: localization changes cannot alter representative CLI, config, primitive, rider, dictionary, or serialization contracts.")
PY
