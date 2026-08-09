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
if rg -q 'SketchOutlinePass|sketchOutlinePass' "$root/viewer3d"; then
  echo "Sketch ink must not use a post-composite geometry pass." >&2
  exit 1
fi
rg -q 'glColor4f\(0\.0f, 0\.0f, 0\.0f, 0\.0f\)' "$renderer"
rg -q 'glPolygonOffset\(-1\.0f, -1\.0f\)' "$renderer"
rg -q 'inkCoverage = 1\.0 - base\.a' "$post_process"
rg -q 'GL_DRAW_FRAMEBUFFER_BINDING' "$post_process"
if rg -q 'glBindFramebuffer\(GL_(DRAW_)?FRAMEBUFFER, 0\)' "$post_process"; then
  echo "Sketch post-processing must restore the captured destination framebuffer." >&2
  exit 1
fi

echo "OK: Sketch uses Standard mesh lighting before post-processing and outlines."
