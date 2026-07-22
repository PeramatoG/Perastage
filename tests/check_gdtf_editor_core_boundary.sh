#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
editor_dir="$repo_root/core/gdtf/editor"

if rg -n '#include <wx/|#include "gui/|#include "fixturetablepanel|#include "trusstablepanel|ConfigManager::Get|GetDefaultGuiConfigServices' "$editor_dir"; then
  echo "GDTF editor core boundary must not include GUI widgets or global config access." >&2
  exit 1
fi
