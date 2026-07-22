#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
panel_cpp="$repo_root/gui/layoutviewerpanel.cpp"
panel_h="$repo_root/gui/layoutviewerpanel.h"
mainwindow_cpp="$repo_root/gui/mainwindow.cpp"
mainwindow_layout_cpp="$repo_root/gui/mainwindow_layout.cpp"

for required in "$panel_cpp" "$panel_h" "$mainwindow_cpp" "$mainwindow_layout_cpp"; do
  if [[ ! -f "$required" ]]; then
    echo "Missing required Layout Viewer fit lifecycle file: ${required#$repo_root/}" >&2
    exit 1
  fi
done

if ! rg -q "void RequestFitToViewport\(\);" "$panel_h"; then
  echo "LayoutViewerPanel must expose an explicit RequestFitToViewport API" >&2
  exit 1
fi

request_body="$(run_test_python - "$panel_cpp" <<'PY'
import re
import sys
text = open(sys.argv[1], encoding='utf-8').read()
match = re.search(r'void LayoutViewerPanel::RequestFitToViewport\(\) \{[\s\S]*?\n\}', text)
if not match:
    sys.exit(1)
print(match.group(0))
PY
)"

for token in "pendingFitOnResize = true" "SchedulePendingFitToViewport"; do
  if [[ "$request_body" != *"$token"* ]]; then
    echo "RequestFitToViewport is missing expected token: $token" >&2
    exit 1
  fi
done

ready_body="$(run_test_python - "$panel_cpp" <<'PY'
import re
import sys
text = open(sys.argv[1], encoding='utf-8').read()
match = re.search(r'bool LayoutViewerPanel::IsViewportReadyForAutomaticFit\(\) const \{[\s\S]*?\n\}', text)
if not match:
    sys.exit(1)
print(match.group(0))
PY
)"

for token in "GetLogicalClientSize" "kMinimumStableFitSizePx" "IsShownOnScreen"; do
  if [[ "$ready_body" != *"$token"* ]]; then
    echo "Automatic fit readiness must verify stable visible client geometry: $token" >&2
    exit 1
  fi
done

attempt_body="$(run_test_python - "$panel_cpp" <<'PY'
import re
import sys
text = open(sys.argv[1], encoding='utf-8').read()
match = re.search(r'bool LayoutViewerPanel::TryCompletePendingFitToViewport\(\) \{[\s\S]*?\n\}', text)
if not match:
    sys.exit(1)
print(match.group(0))
PY
)"

for token in "pendingFitOnResize" "IsViewportReadyForAutomaticFit" "ResetViewToFit" "pendingFitOnResize = false" "RequestRenderRebuild" "Refresh"; do
  if [[ "$attempt_body" != *"$token"* ]]; then
    echo "Automatic fit completion is missing expected token: $token" >&2
    exit 1
  fi
done

schedule_body="$(run_test_python - "$panel_cpp" <<'PY'
import re
import sys
text = open(sys.argv[1], encoding='utf-8').read()
match = re.search(r'void LayoutViewerPanel::SchedulePendingFitToViewport\(\) \{[\s\S]*?\n\}', text)
if not match:
    sys.exit(1)
print(match.group(0))
PY
)"

for token in "deferredFitToViewportScheduled_" "wxWeakRef<LayoutViewerPanel>" "CallAfter" "TryCompletePendingFitToViewport"; do
  if [[ "$schedule_body" != *"$token"* ]]; then
    echo "Deferred automatic fit scheduling is missing expected token: $token" >&2
    exit 1
  fi
done

onsize_body="$(run_test_python - "$panel_cpp" <<'PY'
import re
import sys
text = open(sys.argv[1], encoding='utf-8').read()
match = re.search(r'void LayoutViewerPanel::OnSize\(wxSizeEvent &\) \{[\s\S]*?\n\}', text)
if not match:
    sys.exit(1)
print(match.group(0))
PY
)"

if [[ "$onsize_body" != *"TryCompletePendingFitToViewport"* ]]; then
  echo "OnSize must consume pending fits only through the guarded completion helper" >&2
  exit 1
fi
if [[ "$onsize_body" == *"ResetViewToFit"* ]]; then
  echo "OnSize must not bypass automatic-fit readiness checks" >&2
  exit 1
fi

run_test_python - "$panel_cpp" <<'PYCHECK'
import re
import sys
text = open(sys.argv[1], encoding='utf-8').read()
allowed_functions = {
    'LayoutViewerPanel::RequestFitToViewport',
    'LayoutViewerPanel::OnKeyDown',
    'LayoutViewerPanel::TryCompletePendingFitToViewport',
    'LayoutViewerPanel::ResetViewToFit',
}
violations = []
for match in re.finditer(r'(pendingFitOnResize = true|ResetViewToFit\(\);)', text):
    prefix = text[:match.start()]
    funcs = re.findall(r'(?:bool|void|wxRect|double)\s+(LayoutViewerPanel::\w+)\([^;]*?\)\s*(?:const\s*)?\{', prefix)
    current = funcs[-1] if funcs else '<unknown>'
    if current not in allowed_functions:
        line = prefix.count('\n') + 1
        violations.append(f'{line}: {match.group(1)} in {current}')
if violations:
    print('Routine LayoutViewerPanel refresh paths must not directly request or perform automatic fits:', file=sys.stderr)
    for violation in violations:
        print(violation, file=sys.stderr)
    sys.exit(1)
PYCHECK
if ! rg -q "RequestFitToViewport" "$mainwindow_cpp"; then
  echo "Activating or switching layouts must explicitly request a fit" >&2
  exit 1
fi
if ! rg -q "RequestFitToViewport" "$mainwindow_layout_cpp"; then
  echo "Applying a visible Layout Mode perspective must explicitly request a fit" >&2
  exit 1
fi

echo "OK: Layout Viewer automatic fit lifecycle is explicitly requested, guarded, and deferred."
