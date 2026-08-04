#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
legend="$root/gui/layoutviewerpanel_legend.cpp"

if rg -n 'static[[:space:]].*(unordered_map|FixtureSymbolSvg)' "$legend"; then
  echo "layout legend must not own a function-static fixture SVG cache" >&2
  exit 1
fi
if rg -n 'GetCachedLegendSvgSymbol|CachedSvgSymbolEntry' "$legend"; then
  echo "layout legend must use the managed immutable fixture SVG cache" >&2
  exit 1
fi
