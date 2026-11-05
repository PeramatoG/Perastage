#include "configmanager.h"
#include "guiconfigservices.h"
#include "fixtures/fixture_gdtf_resolution.h"
#include "symbols/project_fixture_symbol_runtime.h"

#include <stdexcept>

namespace {
// The aggregation test exercises production grouping and color resolution.
// Only profile/resource IO and the GUI service bridge are controlled here.
class LegendServices : public IGuiConfigServices {
public:
  IGuiPreferencesService &Preferences() override { throw std::logic_error("Unused"); }
  const IGuiPreferencesService &Preferences() const override { throw std::logic_error("Unused"); }
  IGuiProjectSessionService &Project() override { throw std::logic_error("Unused"); }
  const IGuiProjectSessionService &Project() const override { throw std::logic_error("Unused"); }
  IGuiSelectionService &Selection() override { throw std::logic_error("Unused"); }
  const IGuiSelectionService &Selection() const override { throw std::logic_error("Unused"); }
  IGuiHistoryService &History() override { throw std::logic_error("Unused"); }
  const IGuiHistoryService &History() const override { throw std::logic_error("Unused"); }
  IGuiLayerService &Layers() override { throw std::logic_error("Unused"); }
  const IGuiLayerService &Layers() const override { throw std::logic_error("Unused"); }
  ConfigManager &LegacyConfigManager() override { return ConfigManager::Get(); }
  const ConfigManager &LegacyConfigManager() const override { return ConfigManager::Get(); }
};
}
IGuiConfigServices &GetDefaultGuiConfigServices() {
  static LegendServices services;
  return services;
}
int GetGdtfModeChannelCount(const std::string &, const std::string &mode) {
  return mode.empty() ? -1 : std::stoi(mode);
}
namespace gui::fixtures {
bool ResolveFixtureGdtfDeterministic(const Fixture &fixture, const MvrScene &,
    FixtureGdtfResolution &resolution, std::string &, const std::string &) {
  resolution.selectedPath = fixture.gdtfSpec;
  return true;
}
}
namespace symbols {
std::string BuildProjectFixtureSymbolSource(const Fixture &) { return {}; }
}
