#!/usr/bin/env bash
set -euo pipefail

file="core/gdtf_metadata_summary.cpp"
if rg -n "wxZip|zipstrm|GetNextEntry|description\.xml" "$file" >/dev/null; then
  echo "gdtf_metadata_summary.cpp must use the shared GDTF read services instead of direct ZIP/description.xml traversal." >&2
  exit 1
fi
