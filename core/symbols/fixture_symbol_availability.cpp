#include "fixture_symbol_availability.h"

#include "fixture_symbol_svg_cache.h"

namespace symbol_cache {

// Complete standard and internal resource sets are accepted independently.
FixtureSymbolAvailability
InspectFixtureSymbolAvailability(const std::string &physicalGdtfPath) {
  FixtureSymbolAvailability result;
  const bool inspected =
      InspectFixtureSymbolResources(physicalGdtfPath, result.resources);
  result.storedSvgUsable =
      inspected && (result.resources.standardViewsUsable ||
                    result.resources.perastageViewsUsable);
  result.fallbackRequired = !result.storedSvgUsable;
  result.diagnostic = result.resources.diagnostic;
  return result;
}

// Loads one usable stored SVG through the shared bounded-revision runtime
// cache.
std::shared_ptr<const PerastageSvgSymbolData>
LoadUsableFixtureSymbol(const std::string &physicalGdtfPath,
                        SymbolViewKind view, std::string *errorDetails) {
  return GetFixtureSymbolSvgCache().LookupOrLoad({physicalGdtfPath, view},
                                                 errorDetails);
}

} // namespace symbol_cache
