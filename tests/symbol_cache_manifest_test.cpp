#include "symbol_cache_manifest.h"

#include <cassert>
#include <filesystem>
#include <string>

#include <wx/filename.h>
#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "filesystem_path_utils.h"
#include "json.hpp"

namespace {

namespace fs = std::filesystem;

// Converts a filesystem path to a wxString using the native platform representation.
wxString WxStringFromPath(const fs::path &path) {
#ifdef _WIN32
  return wxString(path.wstring());
#else
  return wxString::FromUTF8(path.string());
#endif
}

// Converts a filesystem path to UTF-8 for project APIs.
std::string ToUtf8String(const fs::path &path) {
  std::u8string utf8 = path.u8string();
  return std::string(utf8.begin(), utf8.end());
}

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

// Writes a minimal project archive containing the symbol cache manifest entry.
bool WriteProjectArchiveWithManifest(const fs::path &projectPath) {
  wxFileName::Mkdir(WxStringFromPath(projectPath.parent_path()), wxS_DIR_DEFAULT,
                    wxPATH_MKDIR_FULL);
  wxFileOutputStream output(WxStringFromPath(projectPath));
  if (!output.IsOk())
    return false;

  wxZipOutputStream zip(output);
  if (!zip.IsOk())
    return false;

  const std::string manifestJson =
      BuildManifestJson(symbol_cache::kCurrentManifestFormatVersion,
                        symbol_cache::kCurrentPerastageSymbolFormatVersion)
          .dump();
  if (!zip.PutNextEntry(symbol_cache::kProjectArchiveEntryName))
    return false;
  zip.Write(manifestJson.data(), manifestJson.size());
  zip.Close();
  return output.IsOk();
}

// Verifies manifest loading from project archives whose paths contain non-ASCII characters.
void TestProjectArchivePathUsesUtf8() {
  const fs::path tempRoot =
      fs::temp_directory_path() / PathUtils::PathFromUtf8("perastage_manifest_viña");
  std::error_code ec;
  fs::remove_all(tempRoot, ec);
  fs::create_directories(tempRoot);

  const fs::path projectPath =
      tempRoot / PathUtils::PathFromUtf8("Escenaro_Metal_ViñaRock_1.pstg");
  assert(WriteProjectArchiveWithManifest(projectPath));

  symbol_cache::SymbolCacheManifest manifest;
  std::string error;
  assert(manifest.LoadFromProjectArchive(ToUtf8String(projectPath), error));
  assert(error.empty());
  assert(manifest.ValidateFixture(BuildRequest()).valid);

  fs::remove_all(tempRoot, ec);
}

} // namespace

// Runs symbol cache manifest validation and update-policy unit tests.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  TestValidManifestSkipsInspection();
  TestMissingManifestFallsBackToInspection();
  TestOutdatedManifestFallsBackToInspection();
  TestChangedGdtfHashFallsBackToInspection();
  TestSuccessfulGenerationUpdatesManifest();
  TestFailedGenerationDoesNotMarkManifestValid();
  TestProjectArchivePathUsesUtf8();
  return 0;
}
