#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
panel_cpp="$repo_root/viewer2d/viewer2dpanel.cpp"
panel_h="$repo_root/viewer2d/viewer2dpanel.h"
report_h="$repo_root/core/diagnostics/DiagnosticReport.h"
report_cpp="$repo_root/core/diagnostics/DiagnosticReport.cpp"
offscreen_cpp="$repo_root/viewer2d/viewer2doffscreenrenderer.cpp"
cmake_tests="$repo_root/tests/CMakeLists.txt"

for required in "$panel_cpp" "$panel_h" "$report_h" "$report_cpp" "$offscreen_cpp"; do
  if [[ ! -f "$required" ]]; then
    echo "Missing required Viewer2D FBO capture diagnostics file: ${required#$repo_root/}" >&2
    exit 1
  fi
done

for token in \
  "Viewer2DCaptureInfo" \
  "Viewer2DCaptureBackend" \
  "RecordViewer2DFboCapture" \
  "RecordViewer2DBackBufferFallback" \
  "RecordViewer2DCaptureFailure" \
  "GetViewer2DCaptureInfo"; do
  if ! rg -n "$token" "$report_h" "$report_cpp" >/dev/null; then
    echo "Viewer2D capture diagnostic state is missing expected token: $token" >&2
    exit 1
  fi
done

render_body="$(python3 - "$panel_cpp" <<'PY'
import re
import sys
text = open(sys.argv[1], encoding='utf-8').read()
match = re.search(r'bool Viewer2DPanel::RenderToRGBA\([\s\S]*?\n}\n\n// Captures RGBA pixels from the legacy back buffer path', text)
if not match:
    sys.exit(1)
print(match.group(0))
PY
)"

if [[ "$render_body" != *"RecordViewer2DFboCapture"* || "$render_body" != *"glReadPixels"* ]]; then
  echo "RenderToRGBA must record FBO success after GL_COLOR_ATTACHMENT0 readback" >&2
  exit 1
fi

if [[ "$render_body" != *"RecordViewer2DBackBufferFallback"* || "$render_body" != *"target.Diagnostic()"* ]]; then
  echo "RenderToRGBA must record fallback usage with the FBO diagnostic reason" >&2
  exit 1
fi

if [[ "$render_body" != *"RecordViewer2DCaptureFailure"* ]]; then
  echo "RenderToRGBA must record definitive capture failures" >&2
  exit 1
fi

for token in \
  "Viewer2D RGBA capture backend" \
  "Viewer2D FBO captures" \
  "Viewer2D fallback captures" \
  "Viewer2D capture failures" \
  "Viewer2D last capture size" \
  "Viewer2D last capture diagnostic"; do
  if ! rg -n "$token" "$report_cpp" >/dev/null; then
    echo "DiagnosticReport must include the Viewer2D capture summary token: $token" >&2
    exit 1
  fi
done

for token in "DiagnosticLogger::Info" "DiagnosticLogger::Warning" "ShouldLogViewer2DBackendTransition"; do
  if ! rg -n "$token" "$report_cpp" >/dev/null; then
    echo "Viewer2D capture diagnostics must use low-noise DiagnosticLogger messages: $token" >&2
    exit 1
  fi
done

if ! rg -n "Viewer2DOffscreenRenderer" "$offscreen_cpp" "$repo_root/viewer2d/viewer2doffscreenrenderer.h" >/dev/null; then
  echo "Viewer2DOffscreenRenderer must remain present" >&2
  exit 1
fi

fallback_body="$(python3 - "$panel_cpp" <<'PY'
import re
import sys
text = open(sys.argv[1], encoding='utf-8').read()
match = re.search(r'bool Viewer2DPanel::RenderToRGBABackBufferFallback\([\s\S]*?\n}\n\n(?:// .*\n)?void Viewer2DPanel::OnPaint', text)
if not match:
    sys.exit(1)
print(match.group(0))
PY
)"

if [[ "$fallback_body" != *"GL_BACK"* ]]; then
  echo "GL_BACK must remain isolated in RenderToRGBABackBufferFallback" >&2
  exit 1
fi

if [[ "$render_body" == *"GL_BACK"* ]]; then
  echo "The main RenderToRGBA FBO path must not read from GL_BACK" >&2
  exit 1
fi

if rg -n "analytics|upload|HTTP|HTTPS|curl|asio|socket" \
    "$report_h" "$report_cpp" "$panel_cpp" "$panel_h" >/dev/null; then
  echo "Viewer2D capture diagnostics must not introduce networking or external telemetry" >&2
  exit 1
fi

if ! rg -n "Viewer2DFboCaptureDiagnostics" "$cmake_tests" >/dev/null; then
  echo "Viewer2D FBO capture diagnostics guard is not registered in tests/CMakeLists.txt" >&2
  exit 1
fi
