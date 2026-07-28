#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
panel_cpp="$repo_root/gui/layoutviewerpanel.cpp"
panel_h="$repo_root/gui/layoutviewerpanel.h"
mainwindow_cpp="$repo_root/gui/mainwindow.cpp"

for required in "$panel_cpp" "$panel_h" "$mainwindow_cpp"; do
  if [[ ! -f "$required" ]]; then
    echo "Missing required Layout project-load cache reset file: ${required#$repo_root/}" >&2
    exit 1
  fi
done

if ! rg -n "ResetPreviewCachesForProjectLoad" "$panel_h" "$panel_cpp" >/dev/null; then
  echo "LayoutViewerPanel must expose and implement ResetPreviewCachesForProjectLoad" >&2
  exit 1
fi

helper_body="$(run_test_python - "$panel_cpp" <<'PY'
import re
import sys
text = open(sys.argv[1], encoding='utf-8').read()
match = re.search(r'void LayoutViewerPanel::ResetPreviewCachesForProjectLoad\(\) \{[\s\S]*?\n\}\n\n// Applies a layout snapshot', text)
if not match:
    sys.exit(1)
print(match.group(0))
PY
)"

for token in \
  "ClearCachedTexture" \
  "pendingPersistentViewCacheJson_.clear" \
  "pendingPersistentViewCacheRasters_.clear" \
  "captureInProgress = false" \
  "renderDirty = true" \
  "renderPending = false" \
  "isLoading = false" \
  "loadingRequested = false" \
  "legendDataDirty_ = true" \
  "legendItems_.clear" \
  "legendDataHash = 0" \
  "hasSceneContentHash = false" \
  "lastSceneContentHash = 0" \
  "viewRenderVersion++" \
  "InvalidateSelectionIndexCache"; do
  if [[ "$helper_body" != *"$token"* ]]; then
    echo "ResetPreviewCachesForProjectLoad is missing expected reset token: $token" >&2
    exit 1
  fi
done

run_test_python - "$mainwindow_cpp" <<'PY'
import re
import sys
text = open(sys.argv[1], encoding='utf-8').read()
match = re.search(r'bool MainWindow::LoadProjectFromPath\([\s\S]*?ApplySavedLayout\(\);', text)
if not match:
    raise SystemExit('MainWindow::LoadProjectFromPath must be locatable through ApplySavedLayout')

load_body = match.group(0)
reset_index = load_body.find('ResetPreviewCachesForProjectLoad')
load_cache_index = load_body.find('LoadPersistentViewCacheFromProject')
apply_index = load_body.find('ApplySavedLayout')

if reset_index < 0:
    raise SystemExit('Project load must reset Layout preview caches before applying the saved layout')
if load_cache_index < 0 or reset_index >= load_cache_index:
    raise SystemExit("Project load must reset stale Layout preview caches before loading the new project's persistent cache")
if apply_index < 0 or reset_index >= apply_index:
    raise SystemExit('Project load must reset Layout preview caches before ApplySavedLayout')
PY
