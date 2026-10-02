#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
if rg -n '#include ".*(command|query|mvr|gdtf|gui|mainwindow|configmanager)' \
  "$root/core/local_ipc"; then
  echo "Local IPC transport must remain independent from scene and frontend logic." >&2
  exit 1
fi
