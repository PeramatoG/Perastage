#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
python_path="$(resolve_test_python)"
reported="$($python_path - <<'PY'
import sys
print(sys.executable)
PY
)"
if [[ -z "$reported" ]]; then
  echo "Resolved Python did not report sys.executable." >&2
  exit 1
fi
case "$reported" in
  *WindowsApps*|*Microsoft*Store*)
    echo "Resolved Python selected the Microsoft Store launcher alias: $reported" >&2
    exit 1
    ;;
esac
echo "OK: resolved Python interpreter is used instead of a launcher alias."
