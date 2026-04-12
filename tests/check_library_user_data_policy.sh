#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "$ROOT_DIR"

# Guardrail: runtime writes must not target installation paths.
# We enforce this on dictionary write entry points where a past regression happened.
for file in core/gdtfdictionary.cpp core/trussdictionary.cpp; do
  if rg -n 'Get(Default|Installed|Base)LibraryPath\("(fixtures|trusses)"\)' "$file" \
      | rg -v 'GetBaseLibraryPath\("(fixtures|trusses)"\) / "(gdtf|truss)_dictionary\.json"'; then
    echo "ERROR: $file contains non-user-data dictionary path resolution." >&2
    exit 1
  fi

done

if ! rg -q 'ProjectUtils::GetWritableLibraryPath\("fixtures"\)' core/gdtfdictionary.cpp; then
  echo "ERROR: core/gdtfdictionary.cpp must resolve editable fixtures data via GetWritableLibraryPath()." >&2
  exit 1
fi

if ! rg -q 'ProjectUtils::GetWritableLibraryPath\("trusses"\)' core/trussdictionary.cpp; then
  echo "ERROR: core/trussdictionary.cpp must resolve editable trusses data via GetWritableLibraryPath()." >&2
  exit 1
fi

echo "OK: dictionary write flows resolve user-data paths and avoid installation writes."
