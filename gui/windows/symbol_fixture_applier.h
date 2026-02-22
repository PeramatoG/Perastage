#pragma once

#include <string>
#include <vector>

#include "symbols/Symbol2D.h"

namespace symbol_preview {

bool ApplySymbolsToFixtureGdtf(const std::vector<symbols::Symbol2D> &symbols,
                               const std::string &fixtureUuid,
                               std::string &errorMessage);

} // namespace symbol_preview
