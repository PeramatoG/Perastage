#pragma once

#include <string>
#include <vector>

#include "fixture.h"
#include "mvrscene.h"
#include "symbols/Symbol2D.h"
#include "symbols/FixtureSymbolDiagnostics.h"

namespace symbol_preview {

struct ApplySymbolsOptions {
  bool updateSceneCopy = true;
  bool updateLibraryCopy = true;
  symbols::FixtureSymbolTimings *timings = nullptr;
};

struct ApplySymbolsResult {
  bool success = false;
  bool sceneUpdated = false;
  bool libraryUpdated = false;
  std::string finalScenePath;
  std::string finalLibraryPath;
  std::string finalSceneFingerprint;
  std::vector<std::string> warnings;
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

// Applies generated SVG views through a boolean contract that cannot report warnings.
// This mutation path must comply with docs/developer/gdtf_mutation_policy.md
// (Perastage audit metadata, revision stamping, and compatibility fallback).
bool ApplySymbolsToFixtureGdtf(const std::vector<symbols::Symbol2D> &symbols,
                               const std::string &fixtureUuid,
                               std::string &errorMessage,
                               const ApplySymbolsOptions &options = {});

// Applies generated SVG views and reports each requested persistence outcome.
ApplySymbolsResult ApplySymbolsToFixtureGdtfWithResult(
    const std::vector<symbols::Symbol2D> &symbols,
    const std::string &fixtureUuid,
    const ApplySymbolsOptions &options = {});

bool SyncFixtureGdtfToLibrary(const Fixture &fixture,
                              const MvrScene &scene,
                              std::string &errorMessage);

} // namespace symbol_preview
