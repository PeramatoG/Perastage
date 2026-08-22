#pragma once

#include <string>

namespace symbols {

enum class FixtureSymbolSource {
  StoredGdtfSvg,
  RenderableGdtfGeometry,
  PerastageFallback
};

struct FixtureSymbolSourceInspection {
  FixtureSymbolSource source = FixtureSymbolSource::PerastageFallback;
  bool storedSvgUsable = false;
  bool renderableGeometry = false;
  std::string diagnostic;
};

FixtureSymbolSourceInspection
InspectFixtureSymbolSource(const std::string &physicalGdtfPath,
                           const std::string &exactGdtfMode);

} // namespace symbols
