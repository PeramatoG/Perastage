#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
panel_cpp="$repo_root/viewer2d/viewer2dpanel.cpp"
panel_h="$repo_root/viewer2d/viewer2dpanel.h"
helper_h="$repo_root/viewer_common/gl_framebuffer_capture_target.h"
helper_cpp="$repo_root/viewer_common/gl_framebuffer_capture_target.cpp"
cmake_file="$repo_root/viewer_common/CMakeLists.txt"

for required in "$panel_cpp" "$panel_h" "$helper_h" "$helper_cpp"; do
  if [[ ! -f "$required" ]]; then
    echo "Missing required Viewer2D RenderToRGBA FBO file: ${required#$repo_root/}" >&2
    exit 1
  fi
done

for token in "FramebufferCaptureTarget" "GL_COLOR_ATTACHMENT0" "BindForReading"; do
  if ! rg -n "$token" "$helper_h" "$helper_cpp" >/dev/null; then
    echo "Framebuffer capture helper is missing expected token: $token" >&2
    exit 1
  fi
done

if ! rg -n "gl_framebuffer_capture_target\.cpp" "$cmake_file" >/dev/null; then
  echo "Framebuffer capture helper source is not registered explicitly in viewer_common/CMakeLists.txt" >&2
  exit 1
fi

render_body="$(run_test_python - "$panel_cpp" <<'PY'
import re
import sys
text = open(sys.argv[1], encoding='utf-8').read()
match = re.search(r'bool Viewer2DPanel::RenderToRGBA\([\s\S]*?\n}\n\n// Captures RGBA pixels from the legacy back buffer path', text)
if not match:
    sys.exit(1)
print(match.group(0))
PY
)"

if [[ "$render_body" != *"FramebufferCaptureTarget"* ]]; then
  echo "Viewer2DPanel::RenderToRGBA must use the dedicated framebuffer capture helper" >&2
  exit 1
fi

if [[ "$render_body" != *"BindForReading"* ]] || ! rg -n "GL_COLOR_ATTACHMENT0" "$helper_h" "$helper_cpp" >/dev/null; then
  echo "Viewer2DPanel::RenderToRGBA must read from GL_COLOR_ATTACHMENT0 through the FBO path" >&2
  exit 1
fi

if [[ "$render_body" == *"GL_BACK"* ]]; then
  echo "Viewer2DPanel::RenderToRGBA must not mix GL_BACK into the main FBO path" >&2
  exit 1
fi

render_internal_body="$(run_test_python - "$panel_cpp" <<'PY'
import re
import sys
text = open(sys.argv[1], encoding='utf-8').read()
match = re.search(r'void Viewer2DPanel::RenderInternal\(bool swapBuffers\) \{[\s\S]*?const RenderSize viewportSize', text)
if not match:
    sys.exit(1)
print(match.group(0))
PY
)"

if [[ "$render_internal_body" != *"if (m_captureFramebufferSizeOverride)"* ||
      "$render_internal_body" != *"glDisable(GL_SCISSOR_TEST)"* ||
      "$render_internal_body" != *"glViewport(0, 0, w, h)"* ]]; then
  echo "RenderInternal must use the active capture framebuffer by disabling scissor and setting the viewport" >&2
  exit 1
fi

capture_branch="$(run_test_python - "$panel_cpp" <<'PY'
import re
import sys
text = open(sys.argv[1], encoding='utf-8').read()
match = re.search(r'if \(m_captureFramebufferSizeOverride\) \{[\s\S]*?\n  \} else \{', text)
if not match:
    sys.exit(1)
print(match.group(0))
PY
)"

if [[ "$capture_branch" == *"ApplyKnownBaseOnscreenState"* ||
      "$capture_branch" == *"glBindFramebuffer(GL_FRAMEBUFFER, 0"* ]]; then
  echo "RenderInternal must not bind framebuffer 0 while a capture framebuffer override is active" >&2
  exit 1
fi

if [[ "$render_internal_body" != *"ApplyKnownBaseOnscreenState"* ]]; then
  echo "RenderInternal must keep the normal onscreen ApplyKnownBaseOnscreenState path" >&2
  exit 1
fi

if ! rg -n "RenderToRGBABackBufferFallback" "$panel_cpp" "$panel_h" >/dev/null; then
  echo "GL_BACK fallback must be isolated in a clearly named RenderToRGBABackBufferFallback helper" >&2
  exit 1
fi

fallback_body="$(run_test_python - "$panel_cpp" <<'PY'
import re
import sys
text = open(sys.argv[1], encoding='utf-8').read()
match = re.search(r'bool Viewer2DPanel::RenderToRGBABackBufferFallback\([\s\S]*?\n}\n\n(?:// .*\n)?void Viewer2DPanel::OnPaint', text)
if not match:
    sys.exit(1)
print(match.group(0))
PY
)"

if [[ "$fallback_body" != *"GL_BACK"* || "$fallback_body" != *"fallback"* ]]; then
  echo "Back-buffer read behavior must be limited to a logged fallback helper" >&2
  exit 1
fi

if ! rg -n "Viewer2DOffscreenRenderer|viewer2doffscreenrenderer" "$repo_root/viewer2d" >/dev/null; then
  echo "Viewer2DOffscreenRenderer must not be removed" >&2
  exit 1
fi

if rg -n "Layout.*(texture|PBO|failure diagnostic)|preview.*(texture|PBO|failure diagnostic)" \
    "$repo_root/viewer2d/pdf" "$repo_root/viewer2d/viewer2dpdfexporter.cpp" >/dev/null; then
  echo "PDF/export/print code must not depend on Layout preview texture, PBO, or failure diagnostics" >&2
  exit 1
fi

if rg -n "\b(Cairo|cairo|Skia|skia|Qt|qt|EGL|egl|pbuffer|Pbuffer)\b" \
    "$repo_root/CMakeLists.txt" "$repo_root/viewer2d" "$repo_root/viewer_common" \
    --glob '!tests/check_viewer2d_render_to_rgba_fbo_boundary.sh' >/dev/null; then
  echo "RenderToRGBA FBO work must not introduce a new rendering backend dependency" >&2
  exit 1
fi
