#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
rasterizer_header="$repo_root/gui/layout_2d_view_rasterizer.h"
rasterizer_source="$repo_root/gui/layout_2d_view_rasterizer.cpp"
panel_header="$repo_root/gui/layoutviewerpanel.h"
preview_source="$repo_root/gui/layoutviewerpanel_view.cpp"
pdf_sources=("$repo_root/viewer2d/pdf" "$repo_root/gui/mainwindow_print.cpp")
placeholder="2D view render unavailable"

for file in "$rasterizer_header" "$rasterizer_source" "$panel_header" "$preview_source"; do
  if [[ ! -f "$file" ]]; then
    echo "Required Layout preview diagnostics file is missing: ${file#$repo_root/}" >&2
    exit 1
  fi
done

for token in \
  "Layout2DViewRasterFailureReason" \
  "diagnosticMessage" \
  "MissingCaptureData" \
  "CommandBufferRasterizationFailed" \
  "RenderToRgbaFailed"; do
  if ! rg -n "$token" "$rasterizer_header" "$rasterizer_source" >/dev/null; then
    echo "Raster failure diagnostic token is missing: $token" >&2
    exit 1
  fi
done

for token in \
  "hasLastRenderFailure" \
  "lastRenderFailureMessage" \
  "lastRenderFailureReason"; do
  if ! rg -n "$token" "$panel_header" "$repo_root/gui/layoutviewerpanel.cpp" >/dev/null; then
    echo "Layout view cache failure state is missing: $token" >&2
    exit 1
  fi
done

if ! rg -n "$placeholder" "$preview_source" >/dev/null; then
  echo "Interactive Layout preview placeholder text is not owned by preview rendering." >&2
  exit 1
fi

for source in "${pdf_sources[@]}"; do
  if rg -n "$placeholder" "$source" >/dev/null; then
    echo "PDF/export path must not reference the preview failure placeholder: ${source#$repo_root/}" >&2
    exit 1
  fi
done
