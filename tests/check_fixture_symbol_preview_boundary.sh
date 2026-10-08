#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
model=("$root/core/symbols/fixture_symbol_preview_model.h" "$root/core/symbols/fixture_symbol_preview_model.cpp")
if rg -n '#include.*(wx/|gui/|viewer2d/|viewer3d/|tinyxml2|gdtf_archive_reader)' "${model[@]}"; then
  echo "Original-resource preview policy must consume inspection without GUI or archive parsing." >&2
  exit 1
fi
if rg -n 'symbolPanels|symbolAvailability|symbolData|OnSymbolPreviewPaint|LoadUsableFixtureSymbol' \
    "$root/gui/fixtureeditdialog.h" "$root/gui/fixtureeditdialog.cpp"; then
  echo "FixtureEditDialog must delegate symbol comparison to the reusable panel." >&2
  exit 1
fi
