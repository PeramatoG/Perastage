#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
workflow="$root/mvr/external_scene_workflow.cpp"

forbidden='ConfigManager|MainWindow|GuiProjectMutationHost|Viewer2D|Viewer3D|wxDialog|GdtfShare|\bIPC\b|\bOSC\b|\bMCP\b'
if rg -n "$forbidden" "$workflow"; then
  echo "External scene workflow contains an application, GUI, or remote-control dependency." >&2
  exit 1
fi
if rg -n '#include ".*(configmanager|mainwindow|viewer|dialog|gdtfnet).*"' "$workflow"; then
  echo "External scene workflow includes a forbidden application header." >&2
  exit 1
fi
if ! rg -q 'perastage_command_text_processor.*perastage_mvr_export.*perastage_mvr_read' \
    "$root/mvr/CMakeLists.txt"; then
  echo "External scene workflow must compose the shared Command/read/export targets." >&2
  exit 1
fi

echo "External scene workflow boundary check passed."
