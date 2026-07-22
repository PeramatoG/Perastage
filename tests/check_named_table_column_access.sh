#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep

readonly files=(
  gui/sceneobjecttablepanel.cpp
  gui/trusstablepanel.cpp
  gui/hoisttablepanel.cpp
  gui/riggingpanel.cpp
  gui/layerpanel.cpp
  gui/summarypanel.cpp
  gui/dictionaryeditdialog.cpp
)

readonly raw_access_pattern='Get(Value|TextValue)\([^[:cntrl:]]*,[[:space:]]*[0-9]+\)|SetValue\([^[:cntrl:]]*,[[:space:]]*[0-9]+\)|GetColumn\([0-9]+\)|columnLabels\[[0-9]+\]|\b(col|column|editedColumn)[[:space:]]*(==|!=|>=|<=)[[:space:]]*[0-9]+|event\.GetColumn\(\)[[:space:]]*(==|!=)[[:space:]]*[0-9]+'

if rg -n "${raw_access_pattern}" "${files[@]}"; then
  echo "ERROR: table model columns must use table-specific named indexes." >&2
  exit 1
fi

echo "OK: table model access uses table-specific named column indexes."
