#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
header="$root/core/inspection/mvr_inspection.h"
source="$root/core/inspection/mvr_inspection.cpp"
cmake="$root/core/CMakeLists.txt"

fail() {
  echo "MVR inspection boundary check failed: $1" >&2
  exit 1
}

grep -Fq 'add_library(perastage_inspection_mvr STATIC' "$cmake" ||
  fail "the production inspection target is missing"
grep -Fq 'perastage_inspection_core perastage_inspection_package' "$cmake" ||
  fail "the service must compose the neutral contract and package inventory"

if grep -Eiq 'wx[A-Z]|wxWidgets|MainWindow|ConfigManager|viewer|download|gdtfnet|canonical' "$header"; then
  fail "the public API exposes GUI, project, network, or mutation dependencies"
fi
if grep -Eiq 'MainWindow|ConfigManager|viewer|gdtfnet|download|ImportAndRegister|ReplaceProject' "$source"; then
  fail "the service may not apply projects or invoke GUI/network workflows"
fi
grep -Fq 'MvrImportMode::ParseOnly' "$source" ||
  fail "the service must use the established parse-only importer path"

echo "MVR inspection boundary check passed"
