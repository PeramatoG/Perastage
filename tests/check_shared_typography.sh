#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if rg -n '#include.*(wx/|GL/|nanovg|configmanager|canvas2d)' core/typography; then
  echo 'Shared typography must remain independent of GUI and rendering backends.' >&2
  exit 1
fi
if rg -n 'm_label(Name|Id|Address)Size|OnShowLabel(Name|Id|Address)' viewer2d/viewer2drenderpanel.*; then
  echo 'Detailed fixture label controls belong to the dedicated editor.' >&2
  exit 1
fi
