#pragma once

#include <string>
#include <vector>

#include "symbols/Symbol2D.h"

class ConfigManager;

namespace tools {

bool CalibrateFixtureSymbolsToPhysicalUnits(ConfigManager &cfg,
                                            const std::string &fixtureUuid,
                                            std::vector<symbols::Symbol2D> &symbols,
                                            std::string &errorMessage);

} // namespace tools
