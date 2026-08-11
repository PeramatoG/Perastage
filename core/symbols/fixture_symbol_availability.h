#pragma once

#include <memory>
#include <string>

#include "PerastageSvgSymbol.h"

namespace symbol_cache {

struct FixtureSymbolAvailability {
  bool storedSvgUsable = false;
  bool fallbackRequired = true;
  std::string diagnostic;
};

FixtureSymbolAvailability
InspectFixtureSymbolAvailability(const std::string &physicalGdtfPath);
std::shared_ptr<const PerastageSvgSymbolData>
LoadUsableFixtureSymbol(const std::string &physicalGdtfPath,
                        SymbolViewKind view,
                        std::string *errorDetails = nullptr);

} // namespace symbol_cache
