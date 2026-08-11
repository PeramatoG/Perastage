#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
service="$root/gui/services/fixture_symbol_preparation_service.cpp"
renderer="$root/viewer3d/render/opaque_fixture_pass.cpp"
worker="$root/gui/services/fixture_symbol_processing_worker.cpp"
capture="$root/gui/tools/scene_model_symbol_capture_service.cpp"

if rg -n 'wxProgressDialog|wxWindowDisabler|wxMessageBox|wxYield|detach\(' "$service"; then
  echo "Automatic fixture symbol preparation must remain cooperative and non-modal." >&2
  exit 1
fi
if rg -n '#include .*mainwindow' "$renderer"; then
  echo "Fixture rendering must not depend directly on MainWindow." >&2
  exit 1
fi
if rg -n 'SymbolCacheManifest|perastage_symbol_cache_manifest|SaveProject|LoadProject|ExportMVR|Print' "$service"; then
  echo "Runtime symbol preparation must not own persistence or export flows." >&2
  exit 1
fi
if rg -n '#include .*wx|ConfigManager::|MainWindow::|Viewer2D|Viewer3D|gl[A-Z]' "$worker"; then
  echo "Fixture symbol processing workers must operate only on copied plain data." >&2
  exit 1
fi
if rg -n 'CaptureSceneModelOrthographicStep|nextCaptureStep|captureSnapshot' "$root/gui"; then
  echo "Automatic generation must not yield between fixture symbol views." >&2
  exit 1
fi

rg -q 'CallAfter' "$service"
rg -q 'wxEVT_IDLE' "$service"
rg -q 'event.RequestMore' "$service"
rg -q 'CaptureSceneModelOrthographicRenders' "$service"
rg -q 'FixtureSymbolCapturePlan' "$capture"
rg -q 'displayLabel' "$service"
rg -q 'exactGdtfMode' "$service"
rg -q 'RequestFixtureSymbolPreparation' "$renderer"
rg -q 'ScopedSceneReplacementLifecycle' "$root/gui/tools/scene_model_symbol_capture_service.cpp"
echo "Fixture symbol preparation remains non-modal, runtime-only, and renderer-decoupled."
