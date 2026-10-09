#pragma once

#include "symbols/project_fixture_symbol_runtime.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

namespace viewer2d::pdf {

inline bool ShouldLoadLegendSvgFromKey(const std::string &symbolKey) {
  if (symbols::IsProjectFixtureSymbolSource(symbolKey))
    return true;
  if (symbolKey.empty())
    return false;
  std::string extension = std::filesystem::path(symbolKey).extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return extension == ".gdtf";
}

inline const std::string &ResolveLegendFixtureSymbolSource(
    const std::string &symbolKey, const std::string &resolvedGdtfPath) {
  if (symbols::IsProjectFixtureSymbolSource(symbolKey))
    return symbolKey;
  return ShouldLoadLegendSvgFromKey(resolvedGdtfPath) ? resolvedGdtfPath : symbolKey;
}

} // namespace viewer2d::pdf
