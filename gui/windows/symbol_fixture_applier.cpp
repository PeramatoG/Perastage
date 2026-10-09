#include "windows/symbol_fixture_applier.h"

#include <utility>

#include "configmanager.h"
#include "fixtures/fixture_gdtf_resolution.h"
#include "guiconfigservices.h"
#include "symbols/fixture_symbol_application.h"
#include "symbols/fixture_symbol_state.h"

namespace symbol_preview {

ApplySymbolsResult ApplySymbolsToFixtureProjectWithResult(
    const std::vector<symbols::Symbol2D> &symbols,
    const std::string &fixtureUuid) {
  ApplySymbolsResult result;
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  auto &scene = cfg.GetScene();
  const auto selected = scene.fixtures.find(fixtureUuid);
  if (selected == scene.fixtures.end()) {
    result.diagnostic = "Could not resolve the selected fixture in the scene.";
    return result;
  }
  gui::fixtures::FixtureGdtfResolution source;
  std::string resolutionError;
  // A retained preview remains applicable if the external definition disappears.
  gui::fixtures::ResolveFixtureGdtfDeterministic(selected->second, scene, source,
                                               resolutionError, "apply-project-symbol");
  auto stagedScene = scene;
  auto stagedStore = cfg.GetProjectFixtureSymbols();
  const auto applied = symbols::ApplyFixtureProjectSymbols(
      stagedScene, stagedStore, fixtureUuid, source.selectedPath,
      symbols, symbols::ProjectFixtureSymbolKind::UserOverride);
  result.success = applied.success;
  result.projectSymbolsUpdated = applied.projectSymbolsUpdated;
  result.unsavedProject = scene.basePath.empty();
  result.diagnostic = applied.diagnostic;
  if (result.success && result.projectSymbolsUpdated) {
    cfg.PushUndoState("apply fixture symbol override");
    scene = std::move(stagedScene);
    cfg.GetProjectFixtureSymbols() = std::move(stagedStore);
    cfg.MarkDirty();
  }
  return result;
}

bool InspectFixtureSymbolState(const Fixture &fixture, const MvrScene &scene,
                               FixtureSymbolInspectionResult &result,
                               std::string &errorMessage) {
  result = {};
  const auto &store = GetDefaultGuiConfigServices()
                          .LegacyConfigManager().GetProjectFixtureSymbols();
  if (store.FindForFixture(fixture)) {
    result.hasValidSvgSymbolSet = true;
    return true;
  }
  gui::fixtures::FixtureGdtfResolution resolution;
  if (!gui::fixtures::ResolveFixtureGdtfDeterministic(
          fixture, scene, resolution, errorMessage, "inspect-symbol"))
    return false;
  result.hasResolvableGdtf = true;
  result.scenePath = resolution.scenePath;
  result.libraryPath = resolution.libraryPath;
  symbols::FixtureSymbolState state;
  if (!symbols::InspectFixtureSymbolState(resolution.selectedPath, state,
                                          errorMessage))
    return false;
  result.editorIsPerastage = state.editorIsPerastage;
  result.hasValidSvgSymbolSet = state.hasValidSvgSymbolSet;
  result.requiresSymbolGeneration = state.requiresSymbolGeneration;
  result.warningMessage = state.warningMessage;
  return true;
}

} // namespace symbol_preview
