#include "symbol_cache_manifest.h"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

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
  std::string error;
  assert(symbol_cache::BuildFixtureSymbolGenerationIdentity(
      "fixtures/fixture.gdtf", "Mode", symbol_cache::kCurrentPerastageSymbolFormatVersion,
      hash, "Fixture Type", request.generationIdentity, error));
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
  symbol_cache::FixtureSymbolGenerationIdentity identity;
  std::string error;
  assert(symbol_cache::BuildFixtureSymbolGenerationIdentity(
      "fixtures/fixture.gdtf", "Mode",
      symbol_cache::kCurrentPerastageSymbolFormatVersion, hash, "Fixture Type",
      identity, error));
  entry["generationIdentityKey"] = identity.key;
  entry["portableGdtfIdentity"] = identity.portableGdtfIdentity;
  entry["gdtfMode"] = identity.gdtfMode;
  entry["symbolFormatVersion"] = identity.symbolFormatVersion;
  entry["semanticFingerprint"] = identity.semanticFingerprint;
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

// Verifies name-keyed version-1 entries remain readable but never validate version 2.
void TestLegacyManifestIsNonAuthoritative() {
  nlohmann::json legacy = BuildManifestJson(
      1, symbol_cache::kCurrentPerastageSymbolFormatVersion);
  legacy["fixtures"][0]["fixtureKey"] = "Fixture Type";
  auto manifest = LoadManifest(legacy);
  assert(manifest.HasLoadedManifest());
  assert(!manifest.IsManifestFormatKnown());
  assert(manifest.Entries().empty());
  const auto result = manifest.ValidateFixture(BuildRequest());
  assert(!result.valid);
  assert(result.status == symbol_cache::ValidationStatus::UnknownManifestVersion);
}

// Verifies that changed GDTF content invalidates the manifest optimization.
void TestChangedGdtfHashFallsBackToInspection() {
  auto manifest = LoadManifest(BuildManifestJson(
      symbol_cache::kCurrentManifestFormatVersion,
      symbol_cache::kCurrentPerastageSymbolFormatVersion, "hash-a"));
  const auto result = manifest.ValidateFixture(BuildRequest("hash-b"));
  assert(!result.valid);
  assert(result.status == symbol_cache::ValidationStatus::MissingEntry);
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


// Writes a GDTF archive with caller-controlled ZIP entry ordering.
bool WriteGdtfArchive(const fs::path &path,
                      const std::vector<std::pair<std::string, std::string>> &entries) {
  wxFileName::Mkdir(WxStringFromPath(path.parent_path()), wxS_DIR_DEFAULT,
                    wxPATH_MKDIR_FULL);
  wxFileOutputStream output(WxStringFromPath(path));
  if (!output.IsOk())
    return false;
  wxZipOutputStream zip(output);
  if (!zip.IsOk())
    return false;
  for (const auto &[name, content] : entries) {
    if (!zip.PutNextEntry(name))
      return false;
    zip.Write(content.data(), content.size());
  }
  zip.Close();
  return output.IsOk();
}

// Verifies semantic GDTF fingerprints ignore ZIP packaging metadata and order.
void TestSemanticFingerprintIgnoresZipPackaging() {
  const fs::path tempRoot = fs::temp_directory_path() / "perastage_semantic_gdtf";
  std::error_code ec;
  fs::remove_all(tempRoot, ec);
  fs::create_directories(tempRoot);

  const std::string description = "<GDTF><FixtureType Name=\"Semantic\"/></GDTF>";
  const std::string svg = "<svg><path d=\"M0 0L1 1\"/></svg>";
  const std::string glb = "glb-bytes";
  const fs::path first = tempRoot / "first.gdtf";
  const fs::path second = tempRoot / "second.gdtf";
  assert(WriteGdtfArchive(first, {{"description.xml", description},
                                  {"models/svg/front.svg", svg},
                                  {"models/glb/body.glb", glb}}));
  assert(WriteGdtfArchive(second, {{"models/glb/body.glb", glb},
                                   {"models/svg/front.svg", svg},
                                   {"description.xml", description}}));

  std::string errorA;
  std::string errorB;
  const std::string hashA = symbol_cache::ComputeGdtfSemanticFingerprint(ToUtf8String(first), errorA);
  const std::string hashB = symbol_cache::ComputeGdtfSemanticFingerprint(ToUtf8String(second), errorB);
  assert(errorA.empty());
  assert(errorB.empty());
  assert(hashA == hashB);
  assert(hashA.rfind("gdtfsymfnv1a64v1:", 0) == 0);

  const auto bytes = [](const std::string &value) {
    return std::vector<std::uint8_t>(value.begin(), value.end());
  };
  std::string memoryError;
  const std::string memoryHash =
      symbol_cache::ComputeGdtfSemanticFingerprintFromEntries(
          {{"MODELS\\SVG\\front.svg", bytes(svg)},
           {"irrelevant.txt", bytes("ignored")},
           {"description.xml", bytes(description)},
           {"models/glb/body.glb", bytes(glb)}},
          memoryError);
  assert(memoryError.empty());
  assert(memoryHash == hashA);

  std::string reorderedError;
  const std::string reorderedHash =
      symbol_cache::ComputeGdtfSemanticFingerprintFromEntries(
          {{"models/glb/body.glb", bytes(glb)},
           {"description.xml", bytes(description)},
           {"models/svg/front.svg", bytes(svg)}},
          reorderedError);
  assert(reorderedError.empty());
  assert(reorderedHash == hashA);

  std::string changedMemoryError;
  assert(symbol_cache::ComputeGdtfSemanticFingerprintFromEntries(
             {{"description.xml", bytes(description)},
              {"models/svg/front.svg", bytes(svg + " ")},
              {"models/glb/body.glb", bytes(glb)}},
             changedMemoryError) != hashA);

  const fs::path changedDescription = tempRoot / "changed_description.gdtf";
  const fs::path changedSvg = tempRoot / "changed_svg.gdtf";
  const fs::path changedModel = tempRoot / "changed_model.gdtf";
  assert(WriteGdtfArchive(changedDescription, {{"description.xml", description + " "},
                                               {"models/svg/front.svg", svg},
                                               {"models/glb/body.glb", glb}}));
  assert(WriteGdtfArchive(changedSvg, {{"description.xml", description},
                                       {"models/svg/front.svg", svg + " "},
                                       {"models/glb/body.glb", glb}}));
  assert(WriteGdtfArchive(changedModel, {{"description.xml", description},
                                         {"models/svg/front.svg", svg},
                                         {"models/glb/body.glb", glb + "2"}}));
  std::string error;
  assert(symbol_cache::ComputeGdtfSemanticFingerprint(ToUtf8String(changedDescription), error) != hashA);
  assert(symbol_cache::ComputeGdtfSemanticFingerprint(ToUtf8String(changedSvg), error) != hashA);
  assert(symbol_cache::ComputeGdtfSemanticFingerprint(ToUtf8String(changedModel), error) != hashA);

  symbol_cache::SymbolCacheManifest manifest;
  auto request = BuildRequest("fnv1a64:e4092621d6b68c8c:297001");
  manifest.MarkFixtureSymbolsValid(request, "2026-06-01T00:00:00Z");
  auto semanticRequest = BuildRequest(hashA);
  const auto result = manifest.ValidateFixture(semanticRequest);
  assert(!result.valid);
  assert(result.status == symbol_cache::ValidationStatus::MissingEntry);

  const fs::path malformed = tempRoot / "malformed.gdtf";
  std::ofstream bad(malformed, std::ios::binary);
  bad << "not a zip";
  bad.close();
  std::string malformedError;
  assert(symbol_cache::ComputeGdtfSemanticFingerprint(ToUtf8String(malformed), malformedError).empty());
  assert(!malformedError.empty());

  fs::remove_all(tempRoot, ec);
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
  TestLegacyManifestIsNonAuthoritative();
  TestChangedGdtfHashFallsBackToInspection();
  TestSuccessfulGenerationUpdatesManifest();
  TestFailedGenerationDoesNotMarkManifestValid();
  TestProjectArchivePathUsesUtf8();
  TestSemanticFingerprintIgnoresZipPackaging();
  return 0;
}
