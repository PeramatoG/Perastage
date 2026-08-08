#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
capture="$root/gui/tools/scene_model_symbol_capture_service.cpp"

if rg -n 'GetScene\(\)\.(fixtures|trusses|sceneObjects|supports).*(swap|clear)|\.swap\(scene\.(fixtures|trusses|sceneObjects|supports)' "$capture"; then
  echo "Fixture symbol capture must not replace live scene containers." >&2
  exit 1
fi

if rg -n 'ScopedFixtureColorOverride|ScopedSingleModelSceneOverride' "$capture"; then
  echo "Fixture symbol capture must not restore the legacy live-scene overrides." >&2
  exit 1
fi

rg -q 'BuildSceneModelSymbolCaptureSnapshot' "$capture"
rg -q 'SceneDataManager::ScopedSnapshot' "$capture"
rg -q 'auto renderWithIsolatedScene' "$capture"
rg -Fq 'ScopedSnapshot isolatedScene(captureSnapshot)' "$capture"
rg -Fq 'PrepareForSceneReplacement();' "$capture"
echo "Fixture symbol capture isolation boundary is intact."
