#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source_file="$root/gui/inspection/inspector_workspace_panel.cpp"

python3 - "$source_file" <<'PY'
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

for signature in (
    "void InspectorWorkspacePanel::ConfigureNavigation(",
    "void InspectorWorkspacePanel::ShowGdtf(",
    "void InspectorWorkspacePanel::ShowMvr(",
    "void InspectorWorkspacePanel::HandleAsyncResult(",
    "void InspectorWorkspacePanel::ReturnToParentMvr(",
):
    body = function_body(signature)
    for notebook in ("navigation_", "notebook_"):
        for mutation in ("AddPage", "InsertPage", "RemovePage", "DeletePage",
                         "DeleteAllPages"):
            assert f"{notebook}->{mutation}" not in body, (signature, notebook, mutation)

build = function_body("void InspectorWorkspacePanel::BuildLayout(")
assert build.count("navigation_->AddPage") == 2
assert build.count("notebook_->AddPage") == 4
PY

echo "Inspector notebook topology checks passed."
