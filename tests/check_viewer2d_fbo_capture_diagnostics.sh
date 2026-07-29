#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
panel_cpp="$repo_root/viewer2d/viewer2dpanel.cpp"
panel_h="$repo_root/viewer2d/viewer2dpanel.h"
framebuffer_cpp="$repo_root/viewer_common/gl_framebuffer_capture_target.cpp"
report_h="$repo_root/core/diagnostics/DiagnosticReport.h"
report_cpp="$repo_root/core/diagnostics/DiagnosticReport.cpp"
offscreen_cpp="$repo_root/viewer2d/viewer2doffscreenrenderer.cpp"
cmake_tests="$repo_root/tests/CMakeLists.txt"

for required in "$panel_cpp" "$panel_h" "$framebuffer_cpp" "$report_h" "$report_cpp" "$offscreen_cpp"; do
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

run_test_python - "$panel_cpp" "$framebuffer_cpp" "$report_cpp" <<'PY'
import re
import sys


def function_body(path, signature):
    text = open(path, encoding="utf-8").read()
    start = text.find(signature)
    if start < 0:
        raise SystemExit(f"Could not find bounded function: {signature}")
    brace = text.find("{", start)
    if brace < 0:
        raise SystemExit(f"Could not find function body: {signature}")
    depth = 0
    for position in range(brace, len(text)):
        if text[position] == "{":
            depth += 1
        elif text[position] == "}":
            depth -= 1
            if depth == 0:
                return text[start : position + 1]
    raise SystemExit(f"Could not bound function body: {signature}")


def require(condition, message):
    if not condition:
        raise SystemExit(message)


panel = function_body(sys.argv[1], "bool Viewer2DPanel::RenderToRGBA(")
cache = function_body(
    sys.argv[2], "FramebufferCaptureTarget *FramebufferCaptureTargetCache::Acquire("
)
read_binding = function_body(
    sys.argv[2], "void FramebufferCaptureTarget::BindForReading() const"
)
fallback_recorder = function_body(
    sys.argv[3], "void DiagnosticReport::RecordViewer2DBackBufferFallback("
)

acquisition = re.search(
    r"FramebufferCaptureTarget\s*\*\s*(\w+)\s*=\s*"
    r"([\w>.\-]+)->Acquire\s*\(\s*w\s*,\s*h\s*\)",
    panel,
)
require(acquisition, "RenderToRGBA must acquire a FramebufferCaptureTarget from the cache")
target_name, cache_expression = acquisition.groups()
failure_branch = re.search(
    rf"if\s*\(\s*!\s*{re.escape(target_name)}\s*\|\|\s*!\s*"
    rf"{re.escape(target_name)}\s*->\s*IsComplete\s*\(\s*\)\s*\)\s*"
    r"\{([\s\S]*?)\n\s*\}",
    panel,
)
require(failure_branch, "RenderToRGBA must fall back for null or incomplete cache acquisitions")
failure = failure_branch.group(1)
diagnostic_read = re.search(
    rf"(?:const\s+)?std::string\s+(\w+)\s*=\s*{re.escape(cache_expression)}"
    r"->Diagnostic\s*\(\s*\)\s*;",
    failure,
)
require(
    diagnostic_read,
    "RenderToRGBA must read the framebuffer cache diagnostic after acquisition failure",
)
diagnostic_name = diagnostic_read.group(1)
recorder = re.search(
    r"RecordViewer2DBackBufferFallback\s*\(([^;]+)\)\s*;", failure
)
require(recorder, "RenderToRGBA must record fallback usage before invoking the fallback")
recorder_arguments = recorder.group(1)
require(
    re.search(rf"\b{re.escape(diagnostic_name)}\b", recorder_arguments),
    "RenderToRGBA must forward the cache diagnostic to the fallback recorder",
)
require(
    re.search(
        rf"\b{re.escape(diagnostic_name)}\s*\.\s*empty\s*\(\s*\)\s*\?\s*"
        r'"FBO unavailable"\s*:\s*' + rf"{re.escape(diagnostic_name)}\b",
        recorder_arguments,
    ),
    "RenderToRGBA must retain an explicit default reason for an empty cache diagnostic",
)
fallback_call = re.search(r"RenderToRGBABackBufferFallback\s*\(", failure)
require(
    fallback_call and recorder.start() < fallback_call.start(),
    "RenderToRGBA must record fallback usage before invoking the fallback",
)

failure_path = re.search(
    r"if\s*\(\s*!\s*(\w+)\.target\.EnsureSize\s*\([^)]*\)\s*\|\|\s*"
    r"!\s*\1\.target\.IsComplete\s*\(\s*\)\s*\)\s*\{([\s\S]*?)\n\s*\}",
    cache,
)
require(failure_path, "FramebufferCaptureTargetCache::Acquire must reject failed or incomplete targets")
created_name, failed_creation = failure_path.groups()
diagnostic_copy = re.search(
    rf"diagnostic_\s*=\s*{re.escape(created_name)}\.target\.Diagnostic\s*\(\s*\)\s*;",
    failed_creation,
)
null_return = re.search(r"return\s+nullptr\s*;", failed_creation)
require(
    diagnostic_copy and null_return and diagnostic_copy.end() <= null_return.start(),
    "FramebufferCaptureTargetCache::Acquire must preserve the target diagnostic before returning nullptr",
)
clear_positions = [match.start() for match in re.finditer(r"diagnostic_\.clear\s*\(\s*\)\s*;", cache)]
pointer_returns = [match.start() for match in re.finditer(r"return\s+&[^;]+\.target\s*;", cache)]
require(
    len(clear_positions) >= 2
    and len(pointer_returns) >= 2
    and clear_positions[0] < pointer_returns[0]
    and pointer_returns[0] < clear_positions[-1] < pointer_returns[-1],
    "Successful cache acquisition must clear stale failure diagnostics",
)

for token, message in (
    ("BindForRendering", "RenderToRGBA must use the dedicated framebuffer helper"),
    ("BindForReading", "RenderToRGBA must bind the framebuffer helper for reading"),
    ("glReadPixels", "RenderToRGBA must retain pixel readback in the FBO path"),
    ("RecordViewer2DFboCapture", "RenderToRGBA must record successful FBO capture"),
):
    require(token in panel, message)
require(
    panel.index("BindForReading") < panel.index("glReadPixels") < panel.index("RecordViewer2DFboCapture"),
    "RenderToRGBA must record FBO success after framebuffer readback",
)
require("GL_BACK" not in panel, "The main RenderToRGBA FBO path must not read from GL_BACK")
require(
    "GL_COLOR_ATTACHMENT0" in read_binding,
    "FramebufferCaptureTarget::BindForReading must retain GL_COLOR_ATTACHMENT0 as the read source",
)

for reason in (
    "Invalid capture target size",
    "Unable to allocate capture buffer",
    "OpenGL initialization failed",
):
    reason_position = panel.find(f'"{reason}"')
    require(
        reason_position >= 0
        and panel.rfind("RecordViewer2DCaptureFailure", 0, reason_position) >= 0,
        f'RenderToRGBA must record definitive capture failure: {reason}',
    )

for token, message in (
    ("++g_viewer2DCaptureInfo.fallbackCount", "Fallback diagnostics must increment fallbackCount"),
    ("g_viewer2DCaptureInfo.lastDiagnostic = reason", "Fallback diagnostics must store the fallback reason"),
    ("g_viewer2DCaptureInfo.fallbackEverUsed = true", "Fallback diagnostics must mark fallbackEverUsed"),
    ("ShouldLogViewer2DBackendTransition", "Fallback diagnostics must retain low-noise transition logging"),
    ("DiagnosticLogger::Warning", "Fallback diagnostics must use the local diagnostic logger"),
):
    require(token in fallback_recorder, message)
PY

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
