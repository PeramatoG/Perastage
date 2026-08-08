#include "../core/symbols/fixture_symbol_svg_cache.h"

#include "../core/symbol_cache_manifest.h"

namespace symbol_cache {

// Invalidates the fingerprint cache without constructing the presentation cache.
void InvalidateFixtureSymbolCachesForPath(const std::string &physicalGdtfPath) {
  InvalidateGdtfSemanticFingerprintCache(physicalGdtfPath);
}

// Clears the fingerprint cache used by the fixture-applier test.
void ClearFixtureSymbolRuntimeCaches() {
  ClearGdtfSemanticFingerprintCache();
}

} // namespace symbol_cache
