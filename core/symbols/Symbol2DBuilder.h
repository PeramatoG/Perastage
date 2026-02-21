#pragma once

#include <string>
#include <vector>

#include "Symbol2D.h"
#include "symbolcache.h"

namespace symbols {

struct BuildParams {
  float previewStrokeWidthPx = 2.0f;
};

class Symbol2DBuilder {
public:
  static std::vector<Symbol2D>
  BuildForFixtureModelKeys(const std::vector<std::string> &modelKeys,
                           const SymbolDefinitionSnapshot &snapshot,
                           const BuildParams &params = BuildParams{});
};

} // namespace symbols
