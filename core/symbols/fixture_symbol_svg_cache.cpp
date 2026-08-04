#include "fixture_symbol_svg_cache.h"

#include "filesystem_path_utils.h"
#include "symbol_cache_manifest.h"

#include <iterator>
#include <utility>

namespace symbol_cache {
namespace {

constexpr int kSvgParserFormatVersion = 1;

// Builds the content-qualified lookup key without presentation labels.
std::string BuildKey(const std::string &pathKey,
                     const FixtureSymbolSvgRequest &request) {
  return pathKey + "\n" + std::to_string(static_cast<int>(request.view)) +
         "\n" + request.semanticFingerprint + "\n" +
         request.generationIdentityKey + "\n" +
         std::to_string(kSvgParserFormatVersion);
}

} // namespace

// Creates a cache with the production loader unless a test loader is supplied.
FixtureSymbolSvgCache::FixtureSymbolSvgCache(Loader loader)
    : loader_(std::move(loader)) {
  if (!loader_) {
    loader_ = [](const std::string &path, SymbolViewKind view,
                 PerastageSvgSymbolData &data, std::string *error) {
      return LoadPerastageSvgSymbolFromGdtf(path, view, data, error);
    };
  }
}

// Returns an immutable cached symbol or loads it without retaining failures.
FixtureSymbolSvgCache::SymbolHandle
FixtureSymbolSvgCache::LookupOrLoad(const FixtureSymbolSvgRequest &request,
                                    std::string *errorDetails) {
  if (request.physicalGdtfPath.empty())
    return {};
  const std::string pathKey =
      PathUtils::BuildFilesystemIdentityKey(request.physicalGdtfPath);
  const std::string key = BuildKey(pathKey, request);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = entries_.find(key);
    if (found != entries_.end()) {
      ++stats_.positiveHits;
      if (errorDetails)
        errorDetails->clear();
      return found->second.symbol;
    }
    ++stats_.loads;
  }

  PerastageSvgSymbolData loaded;
  std::string localError;
  if (!loader_(request.physicalGdtfPath, request.view, loaded, &localError)) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.loadFailures;
    if (errorDetails)
      *errorDetails = localError;
    return {};
  }
  auto handle =
      std::make_shared<const PerastageSvgSymbolData>(std::move(loaded));
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto [it, inserted] = entries_.emplace(
        key, Entry{pathKey, request.generationIdentityKey, handle});
    if (!inserted)
      handle = it->second.symbol;
    stats_.entries = entries_.size();
  }
  if (errorDetails)
    errorDetails->clear();
  return handle;
}

// Invalidates all parsed symbols addressing one canonical physical archive.
void FixtureSymbolSvgCache::InvalidatePath(
    const std::string &physicalGdtfPath) {
  const std::string pathKey =
      PathUtils::BuildFilesystemIdentityKey(physicalGdtfPath);
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = entries_.begin(); it != entries_.end();) {
    it = it->second.pathKey == pathKey ? entries_.erase(it) : std::next(it);
  }
  ++stats_.pathInvalidations;
  stats_.entries = entries_.size();
}

// Invalidates parsed symbols for one canonical generation identity key.
void FixtureSymbolSvgCache::InvalidateGenerationIdentity(
    const std::string &identityKey) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = entries_.begin(); it != entries_.end();) {
    it = !identityKey.empty() && it->second.generationIdentityKey == identityKey
             ? entries_.erase(it)
             : std::next(it);
  }
  ++stats_.identityInvalidations;
  stats_.entries = entries_.size();
}

// Clears all project/session parsed symbols while preserving diagnostic
// counters.
void FixtureSymbolSvgCache::Clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  entries_.clear();
  ++stats_.clears;
  stats_.entries = 0;
}

// Returns a deterministic snapshot of cache statistics.
FixtureSymbolSvgCacheStats FixtureSymbolSvgCache::GetStats() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return stats_;
}

// Returns the process service whose contents are bounded by project lifecycle
// clears.
FixtureSymbolSvgCache &GetFixtureSymbolSvgCache() {
  static FixtureSymbolSvgCache cache;
  return cache;
}

// Coordinates parsed-symbol and fingerprint invalidation after archive
// replacement.
void InvalidateFixtureSymbolCachesForPath(const std::string &physicalGdtfPath) {
  GetFixtureSymbolSvgCache().InvalidatePath(physicalGdtfPath);
  InvalidateGdtfSemanticFingerprintCache(physicalGdtfPath);
}

// Clears runtime symbol state at a committed project lifecycle transition.
void ClearFixtureSymbolRuntimeCaches() {
  GetFixtureSymbolSvgCache().Clear();
  ClearGdtfSemanticFingerprintCache();
}

} // namespace symbol_cache
