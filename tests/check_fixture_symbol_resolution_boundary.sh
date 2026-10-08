#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
resolver=("$root/core/symbols/fixture_symbol_resolution.h" "$root/core/symbols/fixture_symbol_resolution.cpp")
if rg -n '#include.*(wx/|gui/|viewer2d/|viewer3d/|tinyxml2|gdtf_archive_reader)' "${resolver[@]}"; then
  echo "Fixture symbol resolution must consume inspection without parsing archives or depending on GUI." >&2
  exit 1
fi
if rg -n 'LoadStoredView|BuildViewResourcePaths|Find(Standard|Perastage)View|perastageResources' "$root/core/symbols/fixture_symbol_svg_cache.cpp"; then
  echo "The fixture SVG cache must not own resource candidate policy." >&2
  exit 1
fi
if rg -n 'LoadStoredView|storedView[[:space:]]*=' "$root/core/symbols/PerastageSvgSymbol.cpp"; then
  echo "The SVG loader must delegate view fallback to the shared resolver." >&2
  exit 1
fi
