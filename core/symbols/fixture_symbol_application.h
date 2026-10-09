#pragma once

#include "Symbol2D.h"
#include "gdtf_mutation_policy.h"
#include "project_fixture_symbols.h"

#include <string>
#include <vector>

namespace symbols {

struct FixtureSymbolApplicationResult {
  bool success = false;
  bool projectSymbolsUpdated = false;
  bool fixtureReferencesUpdated = false;
  bool standardGdtfUpdated = false;
  bool symbolBindingConflict = false;
  std::string publishedGdtfPath;
  std::vector<std::string> warnings;
  std::string diagnostic;
};

// Project and standard output are deliberately separate serialization products.
bool BuildProjectFixtureSymbolBundle(
    const std::vector<Symbol2D> &symbols, ProjectFixtureSymbolKind kind,
    const std::string &sourceGdtfPath, ProjectFixtureSymbolBundle &bundle,
    std::string &errorMessage);

FixtureSymbolApplicationResult ApplyFixtureProjectSymbols(
    MvrScene &scene, ProjectFixtureSymbolStore &store,
    const std::string &fixtureUuid, const std::string &sourceGdtfPath,
    const std::vector<Symbol2D> &symbols, ProjectFixtureSymbolKind kind);

// A newly inserted instance inherits only unanimous exact source/mode content.
// Existing bindings remain untouched, including divergent user overrides.
FixtureSymbolApplicationResult InheritFixtureProjectSymbols(
    MvrScene &scene, ProjectFixtureSymbolStore &store,
    const std::string &fixtureUuid);

// Persists a fallback only when a definition has no authoritative bundle, and
// independently completes missing standard views through owned derivatives.
FixtureSymbolApplicationResult PrepareFixtureSymbols(
    MvrScene &scene, ProjectFixtureSymbolStore &store,
    const std::string &fixtureUuid, const std::string &sourceGdtfPath,
    const std::vector<Symbol2D> &symbols, gdtf::MutationPolicy policy);

// Captures the current 200 mm cube's project visual convention. These vectors
// are never candidates for standards completion or device physical metadata.
FixtureSymbolApplicationResult ApplyRuntimeFixtureSymbolPlaceholder(
    MvrScene &scene, ProjectFixtureSymbolStore &store,
    const std::string &fixtureUuid, const std::string &sourceGdtfPath);

bool NeedsAutomaticFixtureSymbolPreparation(
    const Fixture &fixture, const ProjectFixtureSymbolStore &store,
    const std::string &sourceGdtfPath, gdtf::MutationPolicy policy);

} // namespace symbols
