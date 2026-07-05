#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
panel_file="$repo_root/gui/layoutviewerpanel.cpp"
rasterizer_file="$repo_root/gui/layout_2d_view_rasterizer.cpp"

if [[ ! -f "$rasterizer_file" ]]; then
  echo "Missing Layout 2D view rasterizer service: gui/layout_2d_view_rasterizer.cpp" >&2
  exit 1
fi

for token in "RenderCommandBufferCacheToRgba" "RenderToRGBA" "ScopedViewer2DState"; do
  if rg -n "$token" "$panel_file" >/dev/null; then
    echo "LayoutViewerPanel should not own 2D view rasterization token: $token" >&2
    exit 1
  fi
  if ! rg -n "$token" "$rasterizer_file" >/dev/null; then
    echo "Layout 2D view rasterizer is expected to own token: $token" >&2
    exit 1
  fi
done
