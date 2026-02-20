#include "symbols/Symbol2DCache.h"

#include <sstream>

namespace symbols {

std::string SymbolBuildParams::Hash() const {
  std::ostringstream ss;
  ss << renderResolution << ":" << strokeWidthPx << ":" << contourSimplify
     << ":" << lineSimplify << ":" << minLineLength;
  return ss.str();
}

std::optional<SymbolCollection>
Symbol2DCache::TryGet(const std::string &fixtureTypeId,
                      const SymbolBuildParams &params) const {
  const auto key = fixtureTypeId + "|" + params.Hash();
  auto it = entries.find(key);
  if (it == entries.end())
    return std::nullopt;
  return it->second;
}

void Symbol2DCache::Store(const std::string &fixtureTypeId,
                          const SymbolBuildParams &params,
                          const SymbolCollection &symbols) {
  entries[fixtureTypeId + "|" + params.Hash()] = symbols;
}

} // namespace symbols
