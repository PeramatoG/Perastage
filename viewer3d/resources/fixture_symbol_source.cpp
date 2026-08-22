#include "fixture_symbol_source.h"

#include <utility>
#include <vector>

#include "../gdtfloader.h"
#include "symbols/fixture_symbol_availability.h"

namespace symbols {

// Classifies the authoritative symbol source for one resolved GDTF and exact mode.
FixtureSymbolSourceInspection
InspectFixtureSymbolSource(const std::string &physicalGdtfPath,
                           const std::string &exactGdtfMode) {
  const auto availability =
      symbol_cache::InspectFixtureSymbolAvailability(physicalGdtfPath);
  if (availability.storedSvgUsable) {
    return {FixtureSymbolSource::StoredGdtfSvg, true, false,
            "The GDTF contains a complete usable stored SVG set."};
  }

  std::vector<GdtfObject> objects;
  std::string geometryDiagnostic;
  if (LoadGdtf(physicalGdtfPath, objects, exactGdtfMode,
               &geometryDiagnostic) &&
      !objects.empty()) {
    return {FixtureSymbolSource::RenderableGdtfGeometry, false, true,
            "The selected GDTF mode provides renderable geometry."};
  }

  std::string diagnostic =
      "The selected GDTF mode has no renderable geometry; using the "
      "Perastage runtime fallback symbol.";
  if (!geometryDiagnostic.empty())
    diagnostic += " " + geometryDiagnostic;
  return {FixtureSymbolSource::PerastageFallback, false, false,
          std::move(diagnostic)};
}

} // namespace symbols
