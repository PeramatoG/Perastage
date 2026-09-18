#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
service_dir="$repo_root/gui/hoisttable"

forbidden='ConfigManager|GetDefaultGuiConfigServices|IGuiConfigServices|SummaryPanel|RiggingPanel|Viewer2DPanel|Viewer3DPanel|HoistLoadRecalculationPrompt|MainWindow|Dialog|wxGetApp'
if rg -n "$forbidden" "$service_dir/hoist_table_edit_service.h" \
  "$service_dir/hoist_table_edit_service.cpp"; then
  echo "Hoist table edit service crosses a forbidden UI/application boundary." >&2
  exit 1
fi

echo "Hoist table edit service boundary check passed."
