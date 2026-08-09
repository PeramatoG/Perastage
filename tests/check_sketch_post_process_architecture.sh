#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
renderer="$root/viewer3d/render/scenerenderer.cpp"
pipeline="$root/viewer3d/render/render_pipeline.cpp"
post_process="$root/viewer3d/render/sketch_post_process_pass.cpp"

if rg -q 'DrawMeshThreeToneInk|CombinedDirectionalDiffuse|uKeyLightDir|uFillLightDir' \
    "$renderer"; then
  echo "Sketch must not duplicate Standard mesh lighting." >&2
  exit 1
fi

rg -q 'IsSketchBasePassActive' "$renderer"
rg -q 'DrawMesh\(mesh, scale, modelMatrix\)' "$renderer"
rg -q 'BeginSketchPostProcess' "$pipeline"
rg -q 'CompleteSketchPostProcess' "$pipeline"
rg -q 'SetSketchOutlinePassActive\(true\)' "$pipeline"
rg -q 'GL_DRAW_FRAMEBUFFER_BINDING' "$post_process"
if rg -q 'glBindFramebuffer\(GL_(DRAW_)?FRAMEBUFFER, 0\)' "$post_process"; then
  echo "Sketch post-processing must restore the captured destination framebuffer." >&2
  exit 1
fi

echo "OK: Sketch uses Standard mesh lighting before post-processing and outlines."
