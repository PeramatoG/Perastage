#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
doc_file="$repo_root/docs/developer/technical-notes/viewer2d_state_ownership.md"
state_header="$repo_root/viewer2d/viewer2dstate.h"
state_source="$repo_root/viewer2d/viewer2dstate.cpp"
capture_file="$repo_root/gui/layout_2d_view_capture_service.cpp"
rasterizer_file="$repo_root/gui/layout_2d_view_rasterizer.cpp"
pdf_dir="$repo_root/viewer2d/pdf"
layout_panel="$repo_root/gui/layoutviewerpanel.cpp"

if [[ ! -f "$doc_file" ]]; then
  echo "Missing Viewer2D state ownership documentation: docs/developer/technical-notes/viewer2d_state_ownership.md" >&2
  exit 1
fi

for phrase in \
  "runtime state" \
  "user preferences/config state" \
  "project/Layout persistent state" \
  "scoped temporary Layout preview/capture path"; do
  if ! rg -i -n "$phrase" "$doc_file" >/dev/null; then
    echo "Viewer2D state ownership documentation must mention: $phrase" >&2
    exit 1
  fi
done

for token in "ScopedViewer2DState" "CaptureState" "ApplyState" "FromLayoutDefinition" "ToLayoutDefinition" "CaptureLayoutDefinition"; do
  if ! rg -n "$token" "$state_header" "$state_source" >/dev/null; then
    echo "Viewer2D state boundary helper is missing token: $token" >&2
    exit 1
  fi
done

for file in "$capture_file" "$rasterizer_file"; do
  if [[ ! -f "$file" ]]; then
    echo "Missing Layout preview state boundary file: ${file#$repo_root/}" >&2
    exit 1
  fi
  if ! rg -n "ScopedViewer2DState" "$file" >/dev/null; then
    echo "Layout preview capture/rasterization must use scoped temporary Viewer2D state: ${file#$repo_root/}" >&2
    exit 1
  fi
  if ! rg -U -n "ScopedViewer2DState[\s\S]{0,240}false" "$file" >/dev/null; then
    echo "Layout preview temporary state must avoid persisting camera config: ${file#$repo_root/}" >&2
    exit 1
  fi
  if rg -n "SaveUserConfig|SaveProject|SaveAsProject|WriteProject|SerializeProject|SaveLayout" "$file" >/dev/null; then
    echo "Layout preview capture/rasterization must not save user config or project data: ${file#$repo_root/}" >&2
    exit 1
  fi
done

if rg -n "previewTexture|previewPbo|failureDiagnostic|diagnosticMessage|Layout2DViewRasterFailure|persistentRgba|OpenGL texture|PBO" "$pdf_dir" >/dev/null; then
  echo "PDF/export/print code must not depend on interactive Layout preview texture, PBO, cache, or failure diagnostic state" >&2
  exit 1
fi

if rg -n "CaptureFrameNow|RenderToRGBA|ScopedViewer2DState" "$layout_panel" >/dev/null; then
  echo "LayoutViewerPanel must not directly own Viewer2D capture calls or scoped state; use the capture/rasterizer services" >&2
  exit 1
fi
