#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "PerastageSvgSymbol.h"

namespace symbol_cache {

struct FixtureSymbolSvgRequest {
  std::string physicalGdtfPath;
  SymbolViewKind view = SymbolViewKind::Top;
  std::string semanticFingerprint;
  std::string generationIdentityKey;
};

struct FixtureSymbolSvgCacheStats {
  std::size_t positiveHits = 0;
  std::size_t loads = 0;
  std::size_t loadFailures = 0;
  std::size_t pathInvalidations = 0;
  std::size_t identityInvalidations = 0;
  std::size_t clears = 0;
  std::size_t entries = 0;
};

class FixtureSymbolSvgCache {
public:
  using SymbolHandle = std::shared_ptr<const PerastageSvgSymbolData>;
  using Loader = std::function<bool(const std::string &, SymbolViewKind,
                                    PerastageSvgSymbolData &, std::string *)>;

  explicit FixtureSymbolSvgCache(Loader loader = {});
  SymbolHandle LookupOrLoad(const FixtureSymbolSvgRequest &request,
                            std::string *errorDetails = nullptr);
  void InvalidatePath(const std::string &physicalGdtfPath);
  void InvalidateGenerationIdentity(const std::string &identityKey);
  void Clear();
  FixtureSymbolSvgCacheStats GetStats() const;

private:
  struct Entry {
    std::string pathKey;
    std::string generationIdentityKey;
    SymbolHandle symbol;
  };
  Loader loader_;
  mutable std::mutex mutex_;
  std::unordered_map<std::string, Entry> entries_;
  FixtureSymbolSvgCacheStats stats_;
};

FixtureSymbolSvgCache &GetFixtureSymbolSvgCache();
void InvalidateFixtureSymbolCachesForPath(const std::string &physicalGdtfPath);
void ClearFixtureSymbolRuntimeCaches();

} // namespace symbol_cache
