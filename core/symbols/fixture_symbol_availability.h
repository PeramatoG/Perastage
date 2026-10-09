#pragma once

#include <array>
#include <memory>
#include <string>

#include "PerastageSvgSymbol.h"

namespace symbol_cache {

struct FixtureSymbolAvailability {
  bool storedSvgUsable = false;
  bool fallbackRequired = true;
  std::string diagnostic;
  FixtureSymbolResourceInspection resources;
  // Exact standard requests never use another view. Internal requests retain
  // rendering view fallback, which is explicit in each result.
  std::array<FixtureSymbolResolution, 6> standardViews;
  std::array<FixtureSymbolResolution, 6> internalViews;
};

// Accepts a resolved GDTF path or a Core-issued PSTG project symbol source token.
// Project tokens are never normalized or interpreted as filesystem paths.
FixtureSymbolAvailability
InspectFixtureSymbolAvailability(const std::string &physicalGdtfPath);
std::shared_ptr<const PerastageSvgSymbolData>
LoadUsableFixtureSymbol(const std::string &physicalGdtfPath,
                        SymbolViewKind view,
                        std::string *errorDetails = nullptr,
                        FixtureSymbolResolutionPurpose purpose =
                            FixtureSymbolResolutionPurpose::InternalRendering);

} // namespace symbol_cache
