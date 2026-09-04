#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TREE_DOC="$ROOT_DIR/docs/developer/perastage_tree.md"
BASELINE="$ROOT_DIR/docs/developer/repository_structure_baseline.json"

mapfile -t required_dirs < <(
  python3 - "$BASELINE" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as stream:
    baseline = json.load(stream)
print(*baseline["top_level_directories"]["source_modules"], sep="\n")
PY
)
required_dirs+=(packaging cmake library resources third_party tests docs)

missing_dirs=()
for dir in "${required_dirs[@]}"; do
  [[ -d "$ROOT_DIR/$dir" ]] || missing_dirs+=("$dir")
done

if [[ ${#missing_dirs[@]} -gt 0 ]]; then
  echo "ERROR: expected repository directories are missing: ${missing_dirs[*]}" >&2
  exit 1
fi

missing_sections=()
for dir in "${required_dirs[@]}"; do
  if ! rg -q --fixed-strings "$dir/" "$TREE_DOC"; then
    missing_sections+=("$dir/")
  fi
done

if [[ ${#missing_sections[@]} -gt 0 ]]; then
  echo "ERROR: docs/developer/perastage_tree.md is missing sections for existing modules: ${missing_sections[*]}" >&2
  exit 1
fi

if ! rg -q "high-level view" "$TREE_DOC"; then
  echo "ERROR: explicit scope note ('high-level view') is missing." >&2
  exit 1
fi

if ! rg -q "rg --files" "$TREE_DOC"; then
  echo "ERROR: reference to 'rg --files' for full detail is missing." >&2
  exit 1
fi

echo "OK: docs/developer/perastage_tree.md keeps module sections and scope note aligned."
