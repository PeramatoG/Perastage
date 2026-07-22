#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
python_path="$(resolve_test_python)"
reported="$("$python_path" - <<'PY'
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
space_dir="$(mktemp -d "${TMPDIR:-/tmp}/perastage python policy.XXXXXX")"
trap 'rm -rf "$space_dir"' EXIT
space_python="$space_dir/python with spaces"
cat > "$space_python" <<SH
#!/usr/bin/env bash
exec "$python_path" "\$@"
SH
chmod +x "$space_python"
space_report="$(PERASTAGE_TEST_PYTHON="$space_python" resolve_test_python)"
if [[ "$space_report" != *" "* ]]; then
  echo "Resolved Python path-with-spaces coverage did not preserve spaces: $space_report" >&2
  exit 1
fi
PERASTAGE_TEST_PYTHON="$space_python" run_test_python - <<'PY'
print("path with spaces executed")
PY

echo "OK: resolved Python interpreter is quoted and used instead of a launcher alias."
