#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

if rg -n '#include .*gui/|#include "(consolepanel|logindialog)' \
    mvr/mvr_import_package.cpp mvr/mvr_import_package.h; then
  echo "MVR import package acquisition must not depend on GUI code." >&2
  exit 1
fi

if rg -n 'wxZipInputStream|ExtractMvrZip' mvr/mvrimporter.cpp mvr/mvrimporter.h; then
  echo "MvrImporter must delegate archive extraction to mvr_import_package." >&2
  exit 1
fi

echo "MVR import package boundary check passed."
