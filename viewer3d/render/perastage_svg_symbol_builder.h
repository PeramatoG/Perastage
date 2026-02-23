#pragma once

#include <string>

#include "symbolcache.h"

bool TryBuildPerastageSvgSymbolDefinition(const std::string &gdtfPath,
                                          SymbolViewKind viewKind,
                                          uint32_t symbolId,
                                          SymbolDefinition &out);
