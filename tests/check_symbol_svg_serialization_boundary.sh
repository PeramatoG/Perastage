#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
serializer="$root/core/symbols/Symbol2DSvg.cpp"
header="$root/core/symbols/Symbol2DSvg.h"
wrapper="$root/gui/windows/symbol_preview_exporter.cpp"

if rg -n '#include.*(wx/|gui/|viewer2d/|viewer3d/)|fstream|ofstream|wx[A-Z]' \
    "$serializer" "$header"; then
  echo "Pure symbol SVG serialization must not depend on GUI, renderers, or file IO." >&2
  exit 1
fi
if rg -n 'fill-rule|#e0e0e0|#ffffff|viewBox|BuildPoints|<polygon|<path' "$wrapper"; then
  echo "The GUI SVG exporter must delegate serialization policy to Core." >&2
  exit 1
fi
rg -q 'symbols::SerializeSymbolToSvg' "$wrapper"
rg -q 'symbols/Symbol2DSvg.cpp' "$root/core/CMakeLists.txt"
echo "Symbol SVG serialization remains GUI-independent and Core-owned."
