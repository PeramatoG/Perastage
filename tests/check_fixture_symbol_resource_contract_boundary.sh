#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
headers=(
  "$root/core/symbols/fixture_symbol_resource_contract.h"
  "$root/core/symbols/symbol_view_kind.h"
  "$root/core/symbols/PerastageSvgSymbol.h"
)

if rg -n '#include.*(wx/|gui/|viewer2d/|viewer3d/|symbolcache\.h|canvas2d\.h)' "${headers[@]}"; then
  echo "Fixture symbol resource contracts must not depend on GUI or renderer types." >&2
  exit 1
fi

if rg -n 'enum class SymbolViewKind' "$root/viewer2d" "$root/viewer3d" "$root/gui"; then
  echo "SymbolViewKind must have one GUI-independent Core definition." >&2
  exit 1
fi

if rg -n 'InspectRequiredFixtureSvgSet' "$root/gui" "$root/viewer2d" "$root/viewer3d"; then
  echo "Fixture symbol consumers must use per-view resource inspection or availability." >&2
  exit 1
fi
