#pragma once

#include <string>
#include <vector>

#include "fixture.h"
#include "mvrscene.h"
#include "symbols/Symbol2D.h"

namespace symbol_preview {

struct ApplySymbolsResult {
  bool success = false;
  bool projectSymbolsUpdated = false;
  bool unsavedProject = false;
  std::string diagnostic;
};

struct FixtureSymbolInspectionResult {
  bool hasResolvableGdtf = false;
  bool editorIsPerastage = false;
  bool hasValidSvgSymbolSet = false;
  bool requiresSymbolGeneration = false;
  std::string warningMessage;
  std::string scenePath;
  std::string libraryPath;
};

bool InspectFixtureSymbolState(const Fixture &fixture,
                               const MvrScene &scene,
                               FixtureSymbolInspectionResult &result,
                               std::string &errorMessage);

// Manual Apply makes an authoritative project override, including before Save.
ApplySymbolsResult ApplySymbolsToFixtureProjectWithResult(
    const std::vector<symbols::Symbol2D> &symbols,
    const std::string &fixtureUuid);

} // namespace symbol_preview
