#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
archive_files=(
  "$root/mvr/mvr_export_archive_writer.h"
  "$root/mvr/mvr_export_archive_writer.cpp"
  "$root/mvr/mvr_export_transport.h"
  "$root/mvr/mvr_export_transport.cpp"
)

for forbidden in ConfigManager 'gui/' MainWindow Panel Dialog MvrScene Fixture Truss Support GroupObject GdtfDictionary GdtfLoader tinyxml2 XMLDocument XMLElement mvr_xml_; do
  if rg -n --fixed-strings "$forbidden" "${archive_files[@]}"; then
    echo "MVR archive boundary violation: $forbidden" >&2
    exit 1
  fi
done

for forbidden in wxZipOutputStream wxZipEntry PutNextEntry wxFileOutputStream wxRemoveFile wxFileName; do
  if rg -n --fixed-strings "$forbidden" "$root/mvr/mvrexporter.cpp"; then
    echo "MvrExporter still owns archive transport: $forbidden" >&2
    exit 1
  fi
done

rg -q --fixed-strings 'mvr_export_archive::Write' "$root/mvr/mvrexporter.cpp"
rg -q --fixed-strings 'mvr_export_transport::WriteToBuffer' "$root/mvr/mvrexporter.cpp"
echo "MVR export archive boundary check passed."
