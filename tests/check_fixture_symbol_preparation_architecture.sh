#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
service="$root/gui/services/fixture_symbol_preparation_service.cpp"
renderer="$root/viewer3d/render/opaque_fixture_pass.cpp"

if rg -n 'wxProgressDialog|wxWindowDisabler|wxMessageBox|wxYield|detach\(' "$service"; then
  echo "Automatic fixture symbol preparation must remain cooperative and non-modal." >&2
  exit 1
fi
if rg -n '#include .*mainwindow' "$renderer"; then
  echo "Fixture rendering must not depend directly on MainWindow." >&2
  exit 1
fi
if rg -n 'symbol_cache_manifest|SaveProject|LoadProject|ExportMVR|Print' "$service"; then
  echo "Runtime symbol preparation must not own persistence or export flows." >&2
  exit 1
fi

rg -q 'CallAfter' "$service"
rg -q 'FixtureSymbolCapturePlan' "$service"
rg -q 'RequestFixtureSymbolPreparation' "$renderer"
echo "Fixture symbol preparation remains non-modal, runtime-only, and renderer-decoupled."
