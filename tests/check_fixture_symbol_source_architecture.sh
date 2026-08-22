#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
classifier="$root/viewer3d/resources/fixture_symbol_source.cpp"
service="$root/gui/services/fixture_symbol_preparation_service.cpp"
fallback="$root/viewer3d/render/fixture_fallback_visual.cpp"
renderer="$root/viewer3d/render/opaque_fixture_pass.cpp"

rg -q 'LoadGdtf\(physicalGdtfPath, objects, exactGdtfMode' "$classifier"
rg -q '#include "\.\./gdtfloader\.h"' "$classifier"
rg -q 'InspectFixtureSymbolSource' "$service"
rg -q 'FixtureCubeMesh' "$fallback"
if rg -q 'FallbackFixtureCubeMesh' "$renderer"; then
  echo "Renderer must use the shared Perastage fixture fallback visual." >&2
  exit 1
fi

echo "Fixture symbol source classification and fallback ownership remain centralized."
