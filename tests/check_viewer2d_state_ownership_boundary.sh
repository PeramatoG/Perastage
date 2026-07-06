#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
doc_file="$repo_root/docs/viewer2d_state_ownership.md"
state_header="$repo_root/viewer2d/viewer2dstate.h"
state_source="$repo_root/viewer2d/viewer2dstate.cpp"
capture_file="$repo_root/gui/layout_2d_view_capture_service.cpp"
rasterizer_file="$repo_root/gui/layout_2d_view_rasterizer.cpp"
pdf_dir="$repo_root/viewer2d/pdf"

if [[ ! -f "$doc_file" ]]; then
  echo "Missing Viewer2D state ownership documentation: docs/viewer2d_state_ownership.md" >&2
  exit 1
fi

for phrase in "Runtime-only state" "User preference/config state" "Project/Layout definition state" "FBO/offscreen cleanup"; do
  if ! rg -n "$phrase" "$doc_file" >/dev/null; then
    echo "Viewer2D state ownership documentation must mention: $phrase" >&2
    exit 1
  fi
done

for token in "Viewer2DStateOwnership" "RuntimeOnly" "UserPreferenceConfig" "ProjectLayoutDefinition" "IsViewer2DUserPreferenceConfigKey"; do
  if ! rg -n "$token" "$state_header" "$state_source" >/dev/null; then
    echo "Viewer2D state ownership map is missing token: $token" >&2
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
  if ! rg -n "ScopedViewer2DState\([^;]*false|false\)" "$file" >/dev/null; then
    echo "Layout preview temporary state must avoid persisting camera config: ${file#$repo_root/}" >&2
    exit 1
  fi
  if rg -n "SaveUserConfig|SaveProject|SaveAsProject|WriteProject|SerializeProject|SaveLayout" "$file" >/dev/null; then
    echo "Layout preview capture/rasterization must not save user config or project data: ${file#$repo_root/}" >&2
    exit 1
  fi
done

if rg -n "preview placeholder|placeholder text|failure diagnostic|diagnosticMessage|Layout2DViewRasterFailure|persistent RGBA|OpenGL texture|PBO" "$pdf_dir" >/dev/null; then
  echo "PDF/export/print code must not depend on interactive Layout preview placeholder or texture failure state" >&2
  exit 1
fi
