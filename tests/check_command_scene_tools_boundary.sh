#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
files=("$root/core/command/command_scene_tools.h"
       "$root/core/command/command_scene_tools.cpp")

if rg -n '#include ".*(configmanager|mainwindow|panel|viewer).*"|#include <wx/' "${files[@]}"; then
  echo "Scene-tool Command Core must remain independent from GUI and ConfigManager." >&2
  exit 1
fi

if ! rg -q 'scene_grouping::GroupSelection' "$root/core/command/command_scene_tools.cpp" ||
   ! rg -q 'scene_node_operations::ConvertFixtureToSupport' "$root/core/command/command_scene_tools.cpp"; then
  echo "Scene-tool commands must delegate to the existing Core services." >&2
  exit 1
fi
