#include "fixture_symbol_availability.h"

#include "fixture_symbol_svg_cache.h"
#include "project_fixture_symbol_runtime.h"

namespace symbol_cache {

// Complete standard and internal resource sets are accepted independently.
FixtureSymbolAvailability
InspectFixtureSymbolAvailability(const std::string &physicalGdtfPath) {
  FixtureSymbolAvailability result;
  const bool inspected =
      symbols::IsProjectFixtureSymbolSource(physicalGdtfPath)
          ? symbols::InspectProjectFixtureSymbolSource(physicalGdtfPath, result.resources)
          : InspectFixtureSymbolResources(physicalGdtfPath, result.resources);
  constexpr std::array views = {SymbolViewKind::Top, SymbolViewKind::Bottom,
      SymbolViewKind::Left, SymbolViewKind::Right, SymbolViewKind::Front, SymbolViewKind::Back};
  for (size_t i = 0; i < views.size(); ++i) {
    result.standardViews[i] = ResolveFixtureSymbolView(result.resources, views[i],
        FixtureSymbolResolutionPurpose::StandardGdtf);
    result.internalViews[i] = ResolveFixtureSymbolView(result.resources, views[i],
        FixtureSymbolResolutionPurpose::InternalRendering);
  }
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
                        SymbolViewKind view, std::string *errorDetails,
                        FixtureSymbolResolutionPurpose purpose) {
  if (symbols::IsProjectFixtureSymbolSource(physicalGdtfPath)) {
    if (purpose == FixtureSymbolResolutionPurpose::StandardGdtf) {
      if (errorDetails)
        *errorDetails = "Project fixture symbols are not standard GDTF resources.";
      return {};
    }
    return symbols::LoadProjectFixtureSymbolSource(physicalGdtfPath, view, errorDetails);
  }
  return GetFixtureSymbolSvgCache().LookupOrLoad({physicalGdtfPath, view, purpose},
                                                 errorDetails);
}

} // namespace symbol_cache
