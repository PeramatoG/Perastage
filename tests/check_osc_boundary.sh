#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"

if rg -n '(command/|ExecutionContext|ConfigManager|MainWindow|GuiProjectMutationHost|local_ipc|live/|mcp|wx[A-Z]|mvrscene)' \
  "$root/core/osc/osc_message."* "$root/core/osc/osc_udp_transport."*; then
  echo "OSC parsing and UDP transport must remain independent from scene and frontend logic." >&2
  exit 1
fi

if rg -n '(ConfigManager|MainWindow|GuiProjectMutationHost|local_ipc|live/|mcp|wx[A-Z])' \
  "$root/core/osc/osc_command_adapter."*; then
  echo "OSC Command mapping must remain independent from GUI and other frontends." >&2
  exit 1
fi
