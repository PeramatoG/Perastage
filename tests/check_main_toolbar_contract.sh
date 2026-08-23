#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
menu_source="$repo_root/gui/mainwindow_menu.cpp"
window_source="$repo_root/gui/mainwindow.cpp"

required_tools=(
  ID_Edit_AddFixture
  ID_Edit_AddTruss
  ID_Edit_AddSceneObject
  ID_Tools_DownloadGdtf
  ID_Tools_ImportRiderText
  ID_View_Layout_Default
  ID_View_Layout_2D
  ID_View_Layout_Mode
)

for tool_id in "${required_tools[@]}"; do
  if ! rg -U -q "(?s)(AddTool|addToolWithDisabledIcon)\\([^;]*${tool_id}" \
      "$menu_source"; then
    echo "Missing required main-toolbar tool: ${tool_id}" >&2
    exit 1
  fi
done

"${PERASTAGE_TEST_PYTHON:-python3}" - "$menu_source" <<'PY'
import re
import sys

text = open(sys.argv[1], encoding="utf-8").read()
start = text.index("void MainWindow::CreateToolBars()")
end = text.index("void MainWindow::CreateMenuBar()", start)
body = text[start:end]
sections = [
    ("fileToolBar", "editToolBar", 7),
    ("editToolBar", "layoutViewsToolBar", 5),
    ("layoutViewsToolBar", "toolsToolBar", 14),
    ("toolsToolBar", "layoutToolBar", 5),
    ("layoutToolBar", "UpdateToolBarAvailability();", 5),
]
for toolbar, next_marker, expected in sections:
    section_start = body.index(toolbar + " =")
    section_end = body.index(next_marker, section_start)
    section = body[section_start:section_end]
    direct = len(re.findall(rf"{toolbar}->AddTool\s*\(", section))
    wrapped = len(re.findall(rf"addToolWithDisabledIcon\s*\(\s*{toolbar}\s*,", section))
    actual = direct + wrapped
    if actual != expected:
        raise SystemExit(
            f"{toolbar} tool count changed: expected {expected}, found {actual}"
        )
PY

availability_body="$({ sed -n '/void MainWindow::UpdateToolBarAvailability()/,/^}/p' "$window_source"; })"
if [[ "$availability_body" == *"DeleteTool("* ||
      "$availability_body" == *"RemoveTool("* ]]; then
  echo "Toolbar availability updates must disable tools without removing them." >&2
  exit 1
fi

echo "OK: main toolbar tools remain registered while availability changes."
