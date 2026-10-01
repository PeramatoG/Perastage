#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
processor_files=(
  "$root/core/command/command_text_processor.h"
  "$root/core/command/command_text_processor.cpp"
)

forbidden='(#[[:space:]]*include[[:space:]]*[<"]([^">]*/)?(gui|app|cli|viewer2d|viewer3d)/|wx[A-Z]|ConfigManager|MainWindow|TablePanel|Viewer[23]D|ConsolePanel|IPC|OSC|MCP)'
if rg -ni "$forbidden" "${processor_files[@]}"; then
  echo "Command text processing must remain independent of frontends, transports, and GUI state." >&2
  exit 1
fi

if rg -n '(selection::ExecuteClear|text::ExecuteSelection|transform::Execute|ParseCommandLine)' \
    "$root/gui/consolepanel.cpp"; then
  echo "ConsolePanel must delegate reusable parsing and semantic dispatch to the text processor." >&2
  exit 1
fi

if ! rg -q 'perastage_command_text_processor' "$root/gui/CMakeLists.txt"; then
  echo "The embedded Console must link the shared text processor." >&2
  exit 1
fi

echo "Command text processor boundary check passed."
