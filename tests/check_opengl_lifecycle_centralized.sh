#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

if ! rg -q "gl_canvas_config.cpp" viewer_common/CMakeLists.txt; then
  echo "viewer_common/gl_canvas_config.cpp is not registered in CMake." >&2
  exit 1
fi

if ! rg -q "gl_context_utils.cpp" viewer_common/CMakeLists.txt; then
  echo "viewer_common/gl_context_utils.cpp is not registered in CMake." >&2
  exit 1
fi

# Keep wxGLCanvas attribute ownership centralized in viewer_common.
violations="$(rg -n "WX_GL_(RGBA|DOUBLEBUFFER|DEPTH_SIZE|SAMPLE_BUFFERS|SAMPLES)" \
  --glob '!viewer_common/gl_canvas_config.cpp' \
  --glob '!tests/check_opengl_lifecycle_centralized.sh' . || true)"
if [[ -n "$violations" ]]; then
  echo "wxGLCanvas attributes must be defined through viewer_common/gl_canvas_config.cpp:" >&2
  echo "$violations" >&2
  exit 1
fi
