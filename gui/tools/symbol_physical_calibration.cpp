#include "tools/symbol_physical_calibration.h"

#include <algorithm>
#include <array>
#include <unordered_map>

#include "configmanager.h"
#include "fixtures/fixture_gdtf_resolution.h"
#include "tools/fixture_geometry_bounds.h"
#include "types.h"

namespace tools {

// Maps generated symbol coordinates to fixture physical dimensions from its
// GDTF.
bool CalibrateFixtureSymbolsToPhysicalUnits(
    ConfigManager &cfg, const std::string &fixtureUuid,
    std::vector<symbols::Symbol2D> &symbols, std::string &errorMessage) {
  const auto &fixtures = cfg.GetScene().fixtures;
  const auto fixtureIt = fixtures.find(fixtureUuid);
  if (fixtureIt == fixtures.end()) {
    errorMessage = "Could not resolve selected fixture for symbol calibration.";
    return false;
  }

  gui::fixtures::FixtureGdtfResolution resolution;
  if (!gui::fixtures::ResolveFixtureGdtfDeterministic(
          fixtureIt->second, cfg.GetScene(), resolution, errorMessage)) {
    return false;
  }
  const std::string gdtfPath = resolution.selectedPath;

  FixtureGeometryBounds fixtureBounds;
  if (!ComputeFixtureGeometryBoundsMm(gdtfPath, fixtureBounds, errorMessage))
    return false;

  return CalibrateFixtureSymbolsToPhysicalUnits(fixtureBounds, symbols,
                                                errorMessage);
}

} // namespace tools
