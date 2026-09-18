#!/usr/bin/env bash
set -euo pipefail

service_dir="gui/trusstable"
forbidden='SummaryPanel|RiggingPanel|Viewer2DPanel|Viewer3DPanel|HoistLoadRecalculationPrompt|MainWindow|dictionary|dialog|ConfigManager::Get|GetDefaultGuiConfigServices|wxGetApp'

if rg -n -i "$forbidden" "$service_dir"; then
  echo "Truss table edit service crosses its mutation boundary." >&2
  exit 1
fi

echo "Truss table edit service boundary check passed."
