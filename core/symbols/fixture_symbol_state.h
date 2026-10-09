#pragma once

#include <string>

namespace symbols {

struct FixtureSymbolState {
  bool editorIsPerastage = false;
  bool hasValidSvgSymbolSet = false;
  bool requiresSymbolGeneration = false;
  std::string warningMessage;
};

// Read-only compatibility inspection. Source bytes are never rewritten.
bool InspectFixtureSymbolState(const std::string &gdtfPath,
                               FixtureSymbolState &state,
                               std::string &errorMessage);

} // namespace symbols
