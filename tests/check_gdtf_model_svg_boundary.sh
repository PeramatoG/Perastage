#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Whole-fixture capture can publish project symbols only.
if rg -n 'StandardGdtfSvgCandidate|SerializeStandardGdtfSvg|CompleteStandardGdtfViews|Prepare.*Derivative|AllowsAutomaticCompletion' \
    "$root/core/symbols/fixture_symbol_application.cpp" \
    "$root/gui/services/fixture_symbol_preparation_service.cpp"; then
  echo "Automatic project preparation must not mutate GDTF Model resources." >&2
  exit 1
fi
if rg -n 'CompleteStandardGdtfViews\(' "$root/gui" "$root/viewer3d" "$root/mvr"; then
  echo "No reviewed model-local automatic SVG generator exists in production." >&2
  exit 1
fi
if rg -n 'modelFile[[:space:]]*=' "$root/core/symbols/standard_gdtf_completion.h"; then
  echo "Standard mutation requires an explicit Model selector." >&2
  exit 1
fi
if rg -n 'ParseFixtureSymbolSvg|PerastageSvgSymbol|wx/|gui/' \
    "$root/core/symbols/gdtf_model_svg_inspection.cpp" \
    "$root/core/symbols/gdtf_model_svg_inspection.h"; then
  echo "Model inspection must separate resource presence from runtime polygon parsing." >&2
  exit 1
fi
if rg -n 'tinyxml2|wxZip|zipstrm|FirstChildElement|Model/@File|models/svg' \
    "$root/gui/gdtf/fixture_symbol_comparison_panel.cpp" \
    "$root/gui/gdtf/fixture_thumbnail_preview.cpp" \
    "$root/gui/gdtf/fixture_part_navigator.cpp"; then
  echo "Preview adapters consume neutral Core/Viewer3D inspection APIs." >&2
  exit 1
fi
rg -q 'GetOrCreateSvg' "$root/gui/gdtf/fixture_symbol_comparison_panel.cpp"
rg -q 'GdtfInspectionObjects' "$root/gui/fixturepreviewpanel.cpp"
if rg -n 'GdtfMutationPolicyPanel' "$root/gui"; then
  echo "The dormant completion policy must not appear in Preferences." >&2
  exit 1
fi
echo "Project captures remain PSTG-only; exact Model inspection and mutation are separate."
