#pragma once

#include "symbols/Symbol2DCache.h"
#include "symbols/Symbol2DTypes.h"

#include <string>

namespace symbols {

class Symbol2DBuilder {
public:
  SymbolCollection BuildForFixture(const std::string &fixtureTypeId,
                                   const std::string &gdtfSpec,
                                   const std::string &sceneBasePath,
                                   const SymbolBuildParams &params = SymbolBuildParams{});

private:
  Symbol2DCache cache;
};

} // namespace symbols
