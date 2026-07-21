#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if ! rg -q 'return dir / "gdtf_dictionary\.json";' core/gdtfdictionary.cpp; then
  echo "ERROR: fixtures dictionary file name contract changed unexpectedly." >&2
  exit 1
fi

if ! rg -q 'return dir / "truss_dictionary\.json";' core/trussdictionary.cpp; then
  echo "ERROR: trusses dictionary file name contract changed unexpectedly." >&2
  exit 1
fi

if ! rg -q 'return ProjectUtils::GetBaseLibraryPath\("fixtures"\) / "gdtf_dictionary\.json";' core/gdtfdictionary.cpp; then
  echo "ERROR: fixtures base dictionary fallback path is missing." >&2
  exit 1
fi

if ! rg -q 'return ProjectUtils::GetBaseLibraryPath\("trusses"\) / "truss_dictionary\.json";' core/trussdictionary.cpp; then
  echo "ERROR: trusses base dictionary fallback path is missing." >&2
  exit 1
fi

echo "OK: dictionary file path contracts (user + base fallback) are intact."
