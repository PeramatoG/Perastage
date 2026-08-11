#include "fixture_symbol_availability.h"

#include "fixture_symbol_svg_cache.h"

namespace symbol_cache {

// Inspects the exact stored SVG set without treating renderer fallback as
// availability.
FixtureSymbolAvailability
InspectFixtureSymbolAvailability(const std::string &physicalGdtfPath) {
  RequiredFixtureSvgSetInspection inspection;
  const bool inspected =
      InspectRequiredFixtureSvgSet(physicalGdtfPath, inspection);
  return {inspected && inspection.usable, !inspected || !inspection.usable,
          inspection.diagnostic};
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
