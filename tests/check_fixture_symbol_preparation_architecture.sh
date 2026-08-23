#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
service="$root/gui/services/fixture_symbol_preparation_service.cpp"
renderer="$root/viewer3d/render/opaque_fixture_pass.cpp"
worker="$root/gui/services/fixture_symbol_processing_worker.cpp"
worker_header="$root/gui/services/fixture_symbol_processing_worker.h"
macos15_workflow="$root/.github/workflows/macos-15-manual-installer.yml"
capture="$root/gui/tools/scene_model_symbol_capture_service.cpp"
capture_header="$root/gui/tools/scene_model_symbol_capture_service.h"
legacy_processing_cpp="$root/gui/tools/scene_model_symbol_processing.cpp"
legacy_processing_header="$root/gui/tools/scene_model_symbol_processing.h"

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
rg -q 'std::jthread' "$worker_header"
rg -q 'std::stop_token' "$worker" "$worker_header"
rg -q 'PERASTAGE_MACOS15_LEGACY_THREAD_COMPAT=ON' "$macos15_workflow"
if rg -n '#if.*__APPLE__|#ifdef[[:space:]]+__APPLE__' "$worker" "$worker_header"; then
  echo "The fixture symbol fallback must not be selected for every macOS build." >&2
  exit 1
fi
if [[ -e "$legacy_processing_cpp" || -e "$legacy_processing_header" ]]; then
  echo "Fixture symbol processing must remain owned by the canonical capture service." >&2
  exit 1
fi
mapfile -t processing_implementations < <(
  rg -l 'SceneModelSymbolCaptureResult ProcessSceneModelOrthographicRenders' \
    "$root/gui" --glob '*.cpp'
)
if [[ "${#processing_implementations[@]}" -ne 1 ||
      "${processing_implementations[0]}" != "$capture" ]]; then
  echo "Fixture symbol processing must have one canonical capture-service implementation." >&2
  exit 1
fi
rg -q 'ProcessSceneModelOrthographicRenders' "$capture_header"
rg -q '#include "tools/scene_model_symbol_capture_service.h"' "$worker"
if rg -n 'WaitUntilIdle|scene_model_symbol_processing' "$worker" "$worker_header" "$service"; then
  echo "Worker backends may change lifecycle primitives but not processing ownership." >&2
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
