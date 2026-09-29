#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
parser_files=(
  "$root/core/command/command_text_parser.h"
  "$root/core/command/command_text_parser.cpp"
)

if rg -n '#include .*?(gui/|app/|configmanager|mainwindow|viewer|tablepanel|mvr)' "${parser_files[@]}"; then
  echo "Command text parser must remain independent of GUI, App, scene IO, and viewers." >&2
  exit 1
fi

if rg -n 'command_text_parser' "$root/core/command/command_contract."{h,cpp} \
    "$root/core/command/command_execution.h" \
    "$root/core/command/command_mutation_transaction."{h,cpp}; then
  echo "Semantic Command Core must not depend on the human text parser." >&2
  exit 1
fi

if rg -n 'command_text_parser\.cpp' "$root/tests/CMakeLists.txt"; then
  echo "Tests must link the production parser target instead of recompiling it." >&2
  exit 1
fi

echo "Command text parser boundary check passed."
