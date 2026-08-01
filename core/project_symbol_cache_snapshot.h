#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "fixture.h"
#include "mvrscene.h"
#include "symbol_cache_manifest.h"

namespace symbol_cache {

struct ProjectFixtureSymbolIdentity {
  std::string fixtureUuid;
  std::string fixtureKey;
  std::string fixtureTypeName;
};

enum class ProjectSymbolSnapshotStatus {
  Validated,
  MissingReference,
  MissingArchive,
  InvalidSymbols,
  Failed,
};

struct ProjectSymbolSnapshotOutcome {
  std::string fixtureKey;
  ProjectSymbolSnapshotStatus status = ProjectSymbolSnapshotStatus::Failed;
  std::string diagnostic;
};

struct ProjectSymbolCacheSnapshotResult {
  bool sceneValid = false;
  SymbolCacheManifest manifest;
  std::vector<ProjectSymbolSnapshotOutcome> outcomes;
  std::vector<std::string> warnings;
  std::size_t validatedCount = 0;
  std::size_t omittedCount = 0;
  std::size_t missingCount = 0;
  std::size_t failedCount = 0;
  std::string errorMessage;
};

// Builds the stable fixture-type key shared by persistence and auto-update planning.
std::string BuildFixtureSymbolCacheKey(const Fixture &fixture);

// Collects immutable fixture identities for exact packaged-scene validation.
std::vector<ProjectFixtureSymbolIdentity>
CollectProjectFixtureSymbolIdentities(const MvrScene &scene);

// Builds a manifest snapshot exclusively from GDTFs packaged in the supplied MVR bytes.
ProjectSymbolCacheSnapshotResult BuildProjectSymbolCacheSnapshot(
    const std::vector<std::uint8_t> &sceneMvrBytes,
    const std::vector<ProjectFixtureSymbolIdentity> &fixtureIdentities,
    const SymbolCacheManifest *provenanceManifest = nullptr,
    const std::string &newEntryTimestampUtc = {});

// Returns the validation requests that still require automatic inspection or generation.
std::vector<std::size_t> PlanFixtureSymbolCacheMisses(
    const SymbolCacheManifest &manifest,
    const std::vector<ValidationRequest> &requests);

} // namespace symbol_cache
