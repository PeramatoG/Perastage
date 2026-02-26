#pragma once

#include <array>
#include <string>

#include "symbolcache.h"

bool TryBuildPerastageSvgSymbolDefinition(const std::string &gdtfPath,
                                          SymbolViewKind viewKind,
                                          uint32_t symbolId,
                                          const std::array<float, 3> &fillRgb,
                                          SymbolDefinition &out);
