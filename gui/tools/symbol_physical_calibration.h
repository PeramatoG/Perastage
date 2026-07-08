#pragma once

#include <string>
#include <vector>

#include "symbols/Symbol2D.h"
#include "tools/fixture_geometry_bounds.h"

class ConfigManager;

namespace tools {

bool CalibrateFixtureSymbolsToPhysicalUnits(
    const FixtureGeometryBounds &fixtureBounds,
    std::vector<symbols::Symbol2D> &symbols,
    std::string &errorMessage);
bool CalibrateFixtureSymbolsToPhysicalUnits(ConfigManager &cfg,
                                            const std::string &fixtureUuid,
                                            std::vector<symbols::Symbol2D> &symbols,
                                            std::string &errorMessage);

} // namespace tools
