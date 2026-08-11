#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
capture="$root/gui/tools/scene_model_symbol_capture_service.cpp"
service="$root/gui/services/fixture_symbol_preparation_service.cpp"
compatibility="$root/gui/tools/scoped_single_model_capture_scene.cpp"

rg -q 'ScopedSingleModelCaptureScene' "$capture"
rg -q 'originalFixtures_\.swap\(scene\.fixtures\)' "$compatibility"
rg -q 'scene\.fixtures\.swap\(originalFixtures_\)' "$compatibility"
rg -Fq 'PrepareForSceneReplacement();' "$capture"
if rg -n 'CaptureSceneModelOrthographicStep|nextCaptureStep|captureSnapshot' "$capture" "$service"; then
  echo "Fixture capture must not yield between orthographic views." >&2
  exit 1
fi
"${PERASTAGE_TEST_PYTHON:?PERASTAGE_TEST_PYTHON is required}" - "$capture" <<'PY'
import pathlib
import sys

text = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
boundary = text.find("CaptureSceneModelOrthographicRenders(")
isolated = text.find("ScopedSingleModelCaptureScene isolatedScene", boundary)
warmup = text.find("RenderToRGBA(", isolated)
loop = text.find("for (const auto &request : requests)", warmup)
fit = text.find("FitViewToScene()", warmup)
render = text.find("RenderToRGBA(", fit)
if min(boundary, isolated, warmup, loop, fit, render) < 0 or not warmup < loop < fit < render:
    raise SystemExit(
        "Fixture capture must warm up and render every view in one isolated scope."
    )
PY
echo "Fixture symbol capture uses one continuous compatibility boundary."
