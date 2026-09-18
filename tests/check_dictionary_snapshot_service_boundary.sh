#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

files=(
  core/dictionary_snapshot_service.h
  core/dictionary_snapshot_service.cpp
)

forbidden='(<wx/|wxWindow|wxDialog|wxMessageBox|DictionaryEditDialog|MainWindow|#include[[:space:]]+["<]gui/|ConfigManager::Get|GetMainWindow)'
if rg -n "$forbidden" "${files[@]}"; then
  echo "Dictionary snapshot service must remain independent of GUI APIs and services." >&2
  exit 1
fi

echo "OK: dictionary snapshot service remains GUI-independent."
