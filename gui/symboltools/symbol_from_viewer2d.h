#pragma once

#include "symbols/Symbol2DTypes.h"

#include <string>
#include <vector>

class Viewer2DPanel;

namespace symboltools {

bool BuildSymbolsFromViewer2DPipeline(Viewer2DPanel &panel,
                                      const std::string &modelKey,
                                      symbols::SymbolCollection &outSymbols,
                                      std::vector<std::string> &outLogLines);

} // namespace symboltools
