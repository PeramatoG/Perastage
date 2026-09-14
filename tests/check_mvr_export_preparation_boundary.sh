#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
header="$repo_root/mvr/mvr_export_preparation.h"
implementation="$repo_root/mvr/mvr_export_preparation.cpp"
exporter="$repo_root/mvr/mvrexporter.cpp"

for forbidden in 'ConfigManager::Get' 'tinyxml2' 'wxZip' 'wxFile' 'ofstream' 'WriteEntry' 'GeneralSceneDescription.xml'; do
  if rg -n "$forbidden" "$header" "$implementation" >/dev/null; then
    echo "MVR export preparation must not own package, XML, GUI, or hidden configuration behavior: $forbidden" >&2
    exit 1
  fi
done

rg -q 'Result Prepare\(const MvrScene &sourceScene, const MvrExportOptions &options\)' "$implementation"
rg -q 'mvr_export_preparation::Prepare\(sourceScene, options\)' "$exporter"
rg -q 'class MvrExporter' "$repo_root/mvr/mvrexporter.h"
if rg -n 'ExportLayerUuid|mvr:layer:' "$exporter" >/dev/null; then
  echo "MVR exporter must consume prepared layer UUIDs without rederiving them." >&2
  exit 1
fi

echo "MVR export preparation boundary check passed."
