#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
files=("$root"/core/capability/*.{h,cpp})
forbidden='ConfigManager|MainWindow|wx(App|Window|Panel)|viewer[23]|MutationHost|ExecutionContext|function<|ipc|osc|mcp'
if rg -n -i "$forbidden" "${files[@]}"; then
  echo "Capability Core contains a forbidden execution, GUI, or transport dependency." >&2
  exit 1
fi
