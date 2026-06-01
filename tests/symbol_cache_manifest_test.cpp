#include "symbol_cache_manifest.h"

#include <cassert>
#include <string>

#include "json.hpp"

namespace {

// Builds a complete cache validation request used by manifest unit tests.
symbol_cache::ValidationRequest BuildRequest(const std::string &hash = "hash-a") {
  symbol_cache::ValidationRequest request;
  request.fixtureKey = "Fixture Type";
  request.fixtureTypeName = "Fixture Type";
  request.gdtfSpec = "fixtures/fixture.gdtf";
  request.gdtfContentHash = hash;
  request.requiredViews = symbol_cache::RequiredPerastageSymbolViews();
  return request;
}

// Loads serialized manifest JSON into a manifest object and asserts success.
symbol_cache::SymbolCacheManifest LoadManifest(const nlohmann::json &json) {
  symbol_cache::SymbolCacheManifest manifest;
  std::string error;
  assert(manifest.LoadFromJsonText(json.dump(), error));
  return manifest;
}

// Creates a JSON manifest entry with overridable version and hash metadata.
nlohmann::json BuildManifestJson(int manifestVersion, int symbolVersion,
                                 const std::string &hash = "hash-a") {
  nlohmann::json entry = nlohmann::json::object();
  entry["fixtureKey"] = "Fixture Type";
  entry["fixtureTypeName"] = "Fixture Type";
  entry["gdtfSpec"] = "fixtures/fixture.gdtf";
  entry["gdtfContentHash"] = hash;
  entry["hasPerastageSymbols"] = true;
  entry["availableViews"] =
      nlohmann::json::array({"top", "bottom", "front", "side"});
  entry["lastGenerationTimestampUtc"] = "2026-06-01T00:00:00Z";

  nlohmann::json root = nlohmann::json::object();
  root["manifestFormatVersion"] = manifestVersion;
  root["perastageSymbolFormatVersion"] = symbolVersion;
  root["fixtures"] = nlohmann::json::array({entry});
  return root;
}

// Simulates the generation policy that only successful applies update the manifest.
void SimulateGeneration(symbol_cache::SymbolCacheManifest &manifest,
                        const symbol_cache::ValidationRequest &request,
                        bool generationSucceeded) {
  if (generationSucceeded)
    manifest.MarkFixtureSymbolsValid(request, "2026-06-01T00:00:00Z");
}

// Verifies that a valid manifest allows the caller to skip deep GDTF inspection.
void TestValidManifestSkipsInspection() {
  auto manifest = LoadManifest(BuildManifestJson(
      symbol_cache::kCurrentManifestFormatVersion,
      symbol_cache::kCurrentPerastageSymbolFormatVersion));
  const auto result = manifest.ValidateFixture(BuildRequest());
  assert(result.valid);
  assert(result.status == symbol_cache::ValidationStatus::Valid);
}

// Verifies that a missing manifest forces the caller to inspect the GDTF normally.
void TestMissingManifestFallsBackToInspection() {
  symbol_cache::SymbolCacheManifest manifest;
  const auto result = manifest.ValidateFixture(BuildRequest());
  assert(!result.valid);
  assert(result.status == symbol_cache::ValidationStatus::MissingManifest);
}

// Verifies that unsupported manifest or symbol versions do not hide GDTF issues.
void TestOutdatedManifestFallsBackToInspection() {
  auto unknownFormat = LoadManifest(BuildManifestJson(
      symbol_cache::kCurrentManifestFormatVersion + 1,
      symbol_cache::kCurrentPerastageSymbolFormatVersion));
  auto unknownResult = unknownFormat.ValidateFixture(BuildRequest());
  assert(!unknownResult.valid);
  assert(unknownResult.status ==
         symbol_cache::ValidationStatus::UnknownManifestVersion);

  auto oldSymbolFormat = LoadManifest(BuildManifestJson(
      symbol_cache::kCurrentManifestFormatVersion,
      symbol_cache::kCurrentPerastageSymbolFormatVersion - 1));
  auto oldResult = oldSymbolFormat.ValidateFixture(BuildRequest());
  assert(!oldResult.valid);
  assert(oldResult.status == symbol_cache::ValidationStatus::OutdatedSymbolFormat);
}

// Verifies that changed GDTF content invalidates the manifest optimization.
void TestChangedGdtfHashFallsBackToInspection() {
  auto manifest = LoadManifest(BuildManifestJson(
      symbol_cache::kCurrentManifestFormatVersion,
      symbol_cache::kCurrentPerastageSymbolFormatVersion, "hash-a"));
  const auto result = manifest.ValidateFixture(BuildRequest("hash-b"));
  assert(!result.valid);
  assert(result.status == symbol_cache::ValidationStatus::GdtfHashChanged);
}

// Verifies that successful generation records a valid manifest entry.
void TestSuccessfulGenerationUpdatesManifest() {
  symbol_cache::SymbolCacheManifest manifest;
  const auto request = BuildRequest();
  SimulateGeneration(manifest, request, true);
  const auto result = manifest.ValidateFixture(request);
  assert(result.valid);
}

// Verifies that failed generation leaves the manifest unable to skip inspection.
void TestFailedGenerationDoesNotMarkManifestValid() {
  symbol_cache::SymbolCacheManifest manifest;
  const auto request = BuildRequest();
  SimulateGeneration(manifest, request, false);
  const auto result = manifest.ValidateFixture(request);
  assert(!result.valid);
  assert(result.status == symbol_cache::ValidationStatus::MissingManifest);
}

} // namespace

// Runs symbol cache manifest validation and update-policy unit tests.
int main() {
  TestValidManifestSkipsInspection();
  TestMissingManifestFallsBackToInspection();
  TestOutdatedManifestFallsBackToInspection();
  TestChangedGdtfHashFallsBackToInspection();
  TestSuccessfulGenerationUpdatesManifest();
  TestFailedGenerationDoesNotMarkManifestValid();
  return 0;
}
