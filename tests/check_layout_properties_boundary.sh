#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$root/tests/test_tool_requirements.sh"
require_ripgrep
if rg -n '^#include.*(LayoutCollection|LayoutManager|configmanager|mainwindow|viewer2d)' \
    "$root/gui/properties_panel.h" "$root/gui/properties_panel.cpp"; then
  echo 'The Properties widgets must remain independent of the document and application.' >&2
  exit 1
fi
if rg -n '^#include.*(configmanager|mainwindow|layoutviewerpanel)' \
    "$root/gui/layout_properties_provider.cpp" "$root/gui/layout_property_edit.cpp"; then
  echo 'Layout property queries and edits must stay independent of the workspace.' >&2
  exit 1
fi
rg -q 'SetElementFrame' "$root/gui/layout_property_edit.cpp"
rg -q 'SetLayoutSelectionHandler' "$root/gui/mainwindow_layout_properties.cpp"
echo 'Layout Properties boundary check passed.'
