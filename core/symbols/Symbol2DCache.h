#pragma once

#include "symbols/Symbol2DTypes.h"

#include <optional>
#include <string>
#include <unordered_map>

namespace symbols {

struct SymbolBuildParams {
  int renderResolution = 1024;
  float strokeWidthPx = 2.0f;
  float contourSimplify = 1.5f;
  float lineSimplify = 1.0f;
  float minLineLength = 6.0f;

  std::string Hash() const;
};

class Symbol2DCache {
public:
  std::optional<SymbolCollection> TryGet(const std::string &fixtureTypeId,
                                         const SymbolBuildParams &params) const;
  void Store(const std::string &fixtureTypeId,
             const SymbolBuildParams &params,
             const SymbolCollection &symbols);

private:
  std::unordered_map<std::string, SymbolCollection> entries;
};

} // namespace symbols
