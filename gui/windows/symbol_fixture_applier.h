#pragma once

#include <string>
#include <vector>

#include "fixture.h"
#include "mvrscene.h"
#include "symbols/Symbol2D.h"

namespace symbol_preview {

struct ApplySymbolsOptions {
  bool updateSceneCopy = true;
  bool updateLibraryCopy = true;
};

struct FixtureSymbolInspectionResult {
  bool hasResolvableGdtf = false;
  bool editorIsPerastage = false;
  bool hasValidSvgSymbolSet = false;
  bool requiresSymbolGeneration = false;
  std::string scenePath;
  std::string libraryPath;
};

bool InspectFixtureSymbolState(const Fixture &fixture,
                               const MvrScene &scene,
                               FixtureSymbolInspectionResult &result,
                               std::string &errorMessage);

bool ApplySymbolsToFixtureGdtf(const std::vector<symbols::Symbol2D> &symbols,
                               const std::string &fixtureUuid,
                               std::string &errorMessage,
                               const ApplySymbolsOptions &options = {});

bool ApplySymbolsToFixtureGdtfForFixture(
    const std::vector<symbols::Symbol2D> &symbols,
    const Fixture &fixture,
    const MvrScene &scene,
    std::string &errorMessage,
    const ApplySymbolsOptions &options = {});

bool SyncFixtureGdtfToLibrary(const Fixture &fixture,
                              const MvrScene &scene,
                              std::string &errorMessage);

} // namespace symbol_preview
