#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# UI adapters may request operations but cannot implement exchange/project formats.
if rg -n 'wxZipOutputStream|RewriteGdtf(WithProof)?[[:space:]]*\(|AppendMutationAuditMetadata|GdtfMutationAudit::(AppendRevision|StampPerastageMutationMetadata|ApplyPhysicalPropertiesWithAudit)' \
    "$root/gui" --glob '*.{cpp,h}'; then
  echo "GUI code must delegate archive mutation and revision policy to Core." >&2
  exit 1
fi
if rg -n 'resources/fixture_symbols/|fixture_bindings|source_fixture_type_id|offset_x_mm|offset_y_mm|data-perastage-' \
    "$root/gui" "$root/viewer2d" "$root/viewer3d" "$root/mvr" --glob '*.{cpp,h}'; then
  echo "Project-symbol format/schema ownership must stay in Core, outside GUI and MVR." >&2
  exit 1
fi

# Legacy path construction belongs to the read-only inspection/loader seam.
if rg -n 'BuildPerastageFixtureSymbolPath|perastage/symbols/' \
    "$root/core" "$root/gui" "$root/viewer2d" "$root/viewer3d" "$root/mvr" \
    --glob '*.{cpp,h}' \
    --glob '!fixture_symbol_resource_contract.h' \
    --glob '!fixture_symbol_resource_contract.cpp' \
    --glob '!PerastageSvgSymbol.cpp' \
    --glob '!fixture_symbol_resource_revision.cpp' \
    --glob '!project_fixture_gdtf_consolidator.cpp' \
    --glob '!fixture_gdtf_derivative_publication.cpp'; then
  echo "Private GDTF symbol paths are compatibility input only; new output must use PSTG or standard GDTF." >&2
  exit 1
fi

core_headers=(
  "$root/core/project_archive_resource.h"
  "$root/core/gdtf_mutation_policy.h"
  "$root/core/symbols/project_fixture_symbols.h"
  "$root/core/symbols/project_fixture_symbol_migration.h"
  "$root/core/symbols/project_fixture_symbol_runtime.h"
  "$root/core/symbols/fixture_symbol_application.h"
  "$root/core/symbols/standard_gdtf_svg.h"
  "$root/core/symbols/standard_gdtf_completion.h"
)
if rg -n '#[[:space:]]*include.*(wx/|gui/|viewer2d/|viewer3d/|json\.hpp|tinyxml2)' "${core_headers[@]}"; then
  echo "Fixture-symbol ownership APIs must expose GUI-independent domain data." >&2
  exit 1
fi
if rg -n '#[[:space:]]*include.*(gui/|viewer2d/|viewer3d/)' \
    "$root/core/symbols/project_fixture_symbols.cpp" \
    "$root/core/symbols/project_fixture_symbol_migration.cpp" \
    "$root/core/symbols/fixture_symbol_application.cpp" \
    "$root/core/symbols/standard_gdtf_completion.cpp" \
    "$root/core/gdtf_publication_resources.cpp"; then
  echo "Symbol persistence/application and GDTF completion must not depend on presentation modules." >&2
  exit 1
fi
if rg -n '#[[:space:]]*include.*(gui/|viewer2d/|viewer3d/)|wx[A-Z]|BuildPerastageFixtureSymbolPath|kPerastageSymbol|data-perastage-|svg_bottom|_bottom\.svg' \
    "$root/core/symbols/standard_gdtf_svg.cpp"; then
  echo "Standard SVG output must have a separate Core serializer without private markers or Bottom." >&2
  exit 1
fi
if rg -n 'SerializeSymbolToSvg|<svg|manifest\.json|index\.json|nlohmann|tinyxml2|wx/zipstrm|wx/wfstream' \
    "$root/gui/windows/symbol_fixture_applier.cpp"; then
  echo "Fixture-symbol Apply must delegate serialization, project binding, and publication to Core." >&2
  exit 1
fi

rg -q 'ApplyFixtureProjectSymbols' "$root/gui/windows/symbol_fixture_applier.cpp"
rg -q 'symbols/project_fixture_symbols.cpp' "$root/core/CMakeLists.txt"
rg -q 'symbols/standard_gdtf_svg.cpp' "$root/core/CMakeLists.txt"
rg -q 'symbols/standard_gdtf_completion.cpp' "$root/core/CMakeLists.txt"
rg -q 'symbols/fixture_symbol_application.cpp' "$root/core/CMakeLists.txt"
# New resets project config: the application mutation preference must be
# restored before preparation scans can run for the next scene.
if ! rg -U -q '(?s)GetPreferencesDialogConfigKeys\(\)[^{]*\{.*?kKeys[^{]*\{[^}]*"gdtf_mutation_policy"' \
    "$root/gui/mainwindow.cpp"; then
  echo "New project must preserve the user's GDTF automatic-completion policy." >&2
  exit 1
fi
echo "Fixture symbols are project-owned; standard completion and serialization remain Core-owned."
