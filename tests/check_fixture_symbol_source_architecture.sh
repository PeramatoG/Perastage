#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
classifier="$root/viewer3d/resources/fixture_symbol_source.cpp"
service="$root/gui/services/fixture_symbol_preparation_service.cpp"
application="$root/core/symbols/fixture_symbol_application.cpp"
geometry="$root/gui/tools/fixture_geometry_bounds.cpp"
fallback="$root/viewer3d/render/fixture_fallback_visual.cpp"
renderer="$root/viewer3d/render/opaque_fixture_pass.cpp"

rg -q 'LoadGdtf\(physicalGdtfPath, objects, exactGdtfMode' "$classifier"
rg -q '#include "\.\./gdtfloader\.h"' "$classifier"
# Preparation must consider project bindings and standard completion separately;
# a complete standard SVG set alone no longer determines project eligibility.
rg -q 'NeedsAutomaticFixtureSymbolPreparation' "$service"
rg -q 'InspectStandardGdtfViews' "$application"
rg -q 'FindForFixture' "$application"
rg -q 'ComputeFixtureGeometryBoundsMm' "$service"
rg -q 'LoadGdtf\(gdtfPath, objects, gdtfMode' "$geometry"
if rg -n 'LoadGdtf[[:space:]]*\(|std::vector<GdtfObject>|InspectStandardGdtfViews[[:space:]]*\(' "$service"; then
  echo "Preparation must delegate geometry loading and mutation eligibility to their shared owners." >&2
  exit 1
fi
rg -q 'FixtureCubeMesh' "$fallback"
if rg -q 'FallbackFixtureCubeMesh' "$renderer"; then
  echo "Renderer must use the shared Perastage fixture fallback visual." >&2
  exit 1
fi

echo "Fixture symbol source classification and fallback ownership remain centralized."
