#pragma once

#include <functional>
#include <string>

namespace symbols {

using FixtureSymbolPreparationRequest =
    std::function<void(const std::string &, const std::string &)>;

void SetFixtureSymbolPreparationRequestHandler(
    FixtureSymbolPreparationRequest handler);
void RequestFixtureSymbolPreparation(const std::string &effectiveGdtfPath,
                                     const std::string &exactGdtfMode);

} // namespace symbols
