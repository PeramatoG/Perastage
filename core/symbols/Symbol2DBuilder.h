#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Symbol2D.h"

struct SymbolDefinitionSnapshot;

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
