#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source_file="$root/gui/inspection/inspector_workspace_panel.cpp"

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

body = function_body("InspectorWorkspacePanel::BeginNestedSourceLoad(")
assert "Cancel(InspectorTaskDomain::Preview)" in body
assert "BeginRetainedSourceRequest" in body
for destructive in ("ClearResult", "ResetPreview", "previewImage_->Hide",
                    "previewText_->Hide", "previewModel_->Hide"):
    assert destructive not in body

body = function_body("void InspectorWorkspacePanel::HandleAsyncResult(")
assert body.find("nestedTransition_.Matches") < body.find("nestedTransition_.Commit")
assert body.find("nestedTransition_.Commit") < body.find("ShowGdtf")
assert body.find("ShowGdtf") < body.find("SetBackNavigationVisible(true)")
PY

echo "Inspector nested transition UI checks passed."
