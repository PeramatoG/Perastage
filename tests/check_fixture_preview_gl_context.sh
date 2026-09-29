#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source_file="$root/gui/fixturepreviewpanel.cpp"

run_test_python - "$source_file" <<'PY'
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text(encoding="utf-8")

def function_body(signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError(f"unterminated function: {signature}")

init = function_body("bool FixturePreviewPanel::InitGL()")
paint = function_body("void FixturePreviewPanel::OnPaint(wxPaintEvent&)")
assert "gl_lifecycle::TrySetCurrent" in init
assert init.find("TrySetCurrent") < init.find("glEnable")
assert "if (!InitGL())" in paint
assert paint.find("if (!InitGL())") < paint.find("Render()")
assert paint.find("Render()") < paint.find("SwapBuffers()")
PY

echo "Fixture preview GL context checks passed."
