#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
capture="$root/gui/tools/scene_model_symbol_capture_service.cpp"
boundary="$root/gui/tools/scene_model_symbol_capture_snapshot.cpp"

if rg -n 'GetScene\(\)\.(fixtures|trusses|sceneObjects|supports).*(swap|clear)|\.swap\(scene\.(fixtures|trusses|sceneObjects|supports)' "$capture"; then
  echo "Fixture symbol capture must not replace live scene containers." >&2
  exit 1
fi

if rg -n 'ScopedFixtureColorOverride|ScopedSingleModelSceneOverride' "$capture"; then
  echo "Fixture symbol capture must not restore the legacy live-scene overrides." >&2
  exit 1
fi

rg -q 'ExecuteSceneModelSymbolCaptureBoundary' "$capture"
rg -q 'BuildSceneModelSymbolCaptureSnapshot' "$boundary"
rg -q 'SceneDataManager::ScopedSnapshot' "$boundary"
rg -Fq 'ScopedSnapshot isolatedScene(snapshot)' "$boundary"
rg -Fq 'PrepareForSceneReplacement();' "$capture"
python3 - "$capture" <<'PY'
import pathlib
import sys

text = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
sync = text.find("SynchronizeSceneForViewFit()")
fit = text.find("FitViewToScene()", sync)
render = text.find("RenderToRGBA(", fit)
if sync < 0 or fit < 0 or render < 0 or not sync < fit < render:
    raise SystemExit(
        "Fixture capture must synchronize snapshot resources and bounds before fitting and rendering."
    )
PY
echo "Fixture symbol capture isolation boundary is intact."
