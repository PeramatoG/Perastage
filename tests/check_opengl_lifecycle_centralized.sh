#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep

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

if ! rg -q "glew_init_utils.cpp" viewer_common/CMakeLists.txt; then
  echo "viewer_common/glew_init_utils.cpp is not registered in CMake." >&2
  exit 1
fi

if rg -q "glew_init_utils.cpp" viewer3d/CMakeLists.txt; then
  echo "GLEW lifecycle utilities must be owned by viewer_common, not viewer3d." >&2
  exit 1
fi

# Finds the first matching include without letting an absent match bypass diagnostics.
find_include_line() {
  local pattern="$1"
  local source_file="$2"
  local match
  match="$(rg -n -m 1 "$pattern" "$source_file" 2>/dev/null || true)"
  printf '%s\n' "${match%%:*}"
}

# Requires GLEW before headers that transitively include the platform OpenGL API.
check_glew_include_order() {
  local source_file="$1"
  local glew_line
  local context_line
  glew_line="$(find_include_line '^#include <GL/glew\.h>[[:space:]]*$' "$source_file")"
  context_line="$(find_include_line '^#include "gl_context_utils\.h"[[:space:]]*$' "$source_file")"
  if ! [[ "$glew_line" =~ ^[0-9]+$ && "$context_line" =~ ^[0-9]+$ ]]; then
    echo "$source_file must include GLEW and gl_context_utils.h." >&2
    exit 1
  fi
  if ((glew_line >= context_line)); then
    echo "$source_file must include GLEW before gl_context_utils.h." >&2
    exit 1
  fi
}

check_glew_include_order viewer2d/viewer2dpanel.cpp
check_glew_include_order gui/layoutviewerpanel.cpp

# Verify include matching remains portable across LF and CRLF checkouts.
include_order_lf="${TMPDIR:-/tmp}/perastage_include_order_lf_$$.cpp"
include_order_crlf="${TMPDIR:-/tmp}/perastage_include_order_crlf_$$.cpp"
trap 'rm -f "$include_order_lf" "$include_order_crlf"' EXIT
printf '#include <GL/glew.h>\n#include "gl_context_utils.h"\n' >"$include_order_lf"
printf '#include <GL/glew.h>\r\n#include "gl_context_utils.h"\r\n' >"$include_order_crlf"
for fixture in "$include_order_lf" "$include_order_crlf"; do
  [[ "$(find_include_line '^#include <GL/glew\.h>[[:space:]]*$' "$fixture")" == "1" &&
     "$(find_include_line '^#include "gl_context_utils\.h"[[:space:]]*$' "$fixture")" == "2" ]] || {
    echo "OpenGL include-order matching must support LF and CRLF files." >&2
    exit 1
  }
done
if [[ -n "$(find_include_line '^#include <missing>[[:space:]]*$' "$include_order_lf")" ]]; then
  echo "Missing OpenGL includes must produce an empty guarded lookup." >&2
  exit 1
fi

if rg -n "viewer3d|../viewer3d" viewer_common --glob '*.{h,cpp}' >/tmp/perastage_viewer_common_viewer3d_refs.txt; then
  echo "viewer_common must not depend on viewer3d-owned headers or sources:" >&2
  cat /tmp/perastage_viewer_common_viewer3d_refs.txt >&2
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
