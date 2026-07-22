#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep

prepare_body=$(python3 - <<'PY'
from pathlib import Path
s=Path('core/dictionary_bundle.cpp').read_text()
start=s.index('PreparedImport PrepareBundleImport(')
end=s.index('// Validates a ZIP bundle', start)
print(s[start:end])
PY
)

if grep -q 'CopyAssetIntoDictionaryStorage\|ApplyImportFromFile\|BackupPath' <<<"${prepare_body}"; then
  echo "PrepareBundleImport must remain side-effect free for active storage." >&2
  exit 1
fi

if ! rg -q 'ApplyPreparedBundleImport\(preparedImport, policy\)' gui/dictionaryeditdialog.cpp; then
  echo "Dictionary Editor ZIP imports must apply prepared bundles only after confirmation." >&2
  exit 1
fi

if ! rg -q 'ValidateBundleFile\(outputZipPath' core/dictionary_bundle.cpp; then
  echo "Portable ZIP exports must validate through the read-only bundle validator." >&2
  exit 1
fi

echo "OK: portable dictionary bundle preparation remains transactional."
