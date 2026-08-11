#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if rg -n 'LoadPerastageSvgSymbolFromGdtf' \
  "$root/gui" "$root/viewer2d" "$root/viewer3d"; then
  echo "Symbol consumers must load stored SVGs through the availability boundary." >&2
  exit 1
fi

if rg -n 'ComputeGdtfSemanticFingerprint' \
  "$root/gui/layoutviewerviewrenderer.cpp" \
  "$root/gui/layoutviewerpanel_legend.cpp" \
  "$root/viewer2d/pdf" "$root/viewer3d/render"; then
  echo "Hot symbol lookup paths must not compute semantic archive fingerprints." >&2
  exit 1
fi

if rg -n 'SymbolCacheManifest|FixtureSymbolGenerationIdentity' \
  "$root/core" "$root/gui" "$root/viewer2d" "$root/viewer3d" "$root/mvr"; then
  echo "Obsolete persistent symbol-manifest identities must not return to production." >&2
  exit 1
fi
