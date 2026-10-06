#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
if rg -n '#include ".*(command|query|mvr|gdtf|gui|mainwindow|configmanager)' \
  "$root/core/local_ipc"; then
  echo "Local IPC transport must remain independent from scene and frontend logic." >&2
  exit 1
fi

if rg -n 'scene\.(objects|object|layers|groups|selection)|target_kind|object_kinds|object_uuids' \
  "$root/core/local_ipc"; then
  echo "Local IPC structured arguments must remain independent from semantic scene operations." >&2
  exit 1
fi

if rg -n '#include ".*(gui/|mainwindow|configmanager|mvr/|gdtf)' \
  "$root/core/live"; then
  echo "Local live execution must not acquire GUI or interchange ownership." >&2
  exit 1
fi
