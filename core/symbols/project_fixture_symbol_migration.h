#pragma once

#include <string>

class MvrScene;
namespace symbols {
class ProjectFixtureSymbolStore;

// Materializes positively identified legacy views without rewriting any GDTF.
// Unknown authored resources and existing project overrides remain authoritative.
bool MaterializeLegacyFixtureSymbols(MvrScene &scene,
    ProjectFixtureSymbolStore &store, const std::string &fixtureLibraryRoot,
    std::string &errorMessage);
} // namespace symbols
