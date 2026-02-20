#pragma once

#include "symbols/Symbol2DTypes.h"

#include <cstdint>

#include <string>
#include <vector>

class Viewer2DPanel;
class ConfigManager;

namespace symboltools {

struct ReferenceImage {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> rgba;
};

struct SymbolReferenceViews {
  symbols::SymbolView view = symbols::SymbolView::Front;
  ReferenceImage shape;
  ReferenceImage line;
};

bool BuildSymbolsFromViewer2DPipeline(Viewer2DPanel &panel,
                                      ConfigManager &configManager,
                                      const std::string &fixtureUuid,
                                      const std::string &modelKey,
                                      symbols::SymbolCollection &outSymbols,
                                      std::vector<std::string> &outLogLines,
                                      std::vector<SymbolReferenceViews> &outReferences);

} // namespace symboltools
