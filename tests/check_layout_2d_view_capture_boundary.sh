#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
panel_file="$repo_root/gui/layoutviewerpanel_view.cpp"
service_file="$repo_root/gui/layout_2d_view_capture_service.cpp"

if [[ ! -f "$service_file" ]]; then
  echo "Missing Layout 2D view capture service: gui/layout_2d_view_capture_service.cpp" >&2
  exit 1
fi

for token in "CaptureFrameNow" "ScopedViewer2DState" "FromLayoutDefinition"; do
  if rg -n "$token" "$panel_file" >/dev/null; then
    echo "LayoutViewerPanel view drawing should not own 2D capture token: $token" >&2
    exit 1
  fi
  if ! rg -n "$token" "$service_file" >/dev/null; then
    echo "Layout 2D view capture service is expected to own token: $token" >&2
    exit 1
  fi
done

if rg -n "GetBottomSymbolCacheSnapshot" "$panel_file" | rg -v "completeCapture|capturePanel" >/dev/null; then
  echo "LayoutViewerPanel should only update captured symbol snapshots from the capture completion callback" >&2
  exit 1
fi
