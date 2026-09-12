#include "mvr_import_resource_resolver.h"

#include "filesystem_path_utils.h"
#include "truss.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {
int dictionaryLookups = 0;
}

namespace mvr {
// Supplies archive-path normalization for this isolated resolver test.
std::string NormalizeImportArchivePath(const std::string &path) {
  std::string normalized = path;
  std::replace(normalized.begin(), normalized.end(), '\\', '/');
  while (normalized.starts_with("./"))
    normalized.erase(0, 2);
  const std::string extension = ".gdtf";
  const size_t extensionPosition = normalized.rfind(extension);
  if (extensionPosition != std::string::npos) {
    size_t trimPosition = extensionPosition;
    while (trimPosition > 0 && normalized[trimPosition - 1] == ' ')
      --trimPosition;
    normalized.erase(trimPosition, extensionPosition - trimPosition);
  }
  return normalized;
}

// Disables primitive aliases because this test exercises file resources.
bool ResolvePrimitiveTokenFromModelRef(const std::string &, std::string &) {
  return false;
}
} // namespace mvr

namespace GdtfDictionary {
// Counts dictionary loads so cache behavior can be verified deterministically.
std::optional<Entry> Get(const std::string &) {
  ++dictionaryLookups;
  return Entry{};
}
} // namespace GdtfDictionary

// Supplies empty GDTF modes for path-only test resources.
std::vector<std::string> GetGdtfModes(const std::string &) { return {}; }

// Supplies an unavailable channel count for path-only test resources.
int GetGdtfModeChannelCount(const std::string &, const std::string &) {
  return -1;
}

// Supplies empty fixture names for path-only test resources.
std::string GetGdtfFixtureName(const std::string &) { return {}; }

// Supplies empty manufacturers for path-only test resources.
std::string GetGdtfFixtureManufacturer(const std::string &) { return {}; }

// Supplies empty fixture identifiers for path-only test resources.
std::string GetGdtfFixtureTypeId(const std::string &) { return {}; }

// Supplies absent physical properties for path-only test resources.
bool GetGdtfProperties(const std::string &, float &, float &) { return false; }

// Supplies failed truss loading for path-only test resources.
bool LoadTrussDefinition(const std::string &, Truss &) { return false; }

// Exercises resource lookup, caching, Unicode paths, and mode fallback order.
int main() {
  const fs::path root =
      fs::temp_directory_path() / fs::path(u8"perastage_resource_ünicode");
  fs::remove_all(root);
  fs::create_directories(root);
  const fs::path exactNamePath = fs::path(u8"Fixture Ä.gdtf");
  const std::string exactName = PathUtils::PathToUtf8(exactNamePath);
  const fs::path exact = root / exactNamePath;
  std::ofstream(exact).put('\n');
  const fs::path unrelated = root / "Unrelated.gdtf";
  std::ofstream(unrelated).put('\n');
  const fs::path canonical = root / "Maker@Canonical Fixture@Perastage.gdtf";
  std::ofstream(canonical).put('\n');
  const fs::path support = root / "Support Fixture.gdtf";
  std::ofstream(support).put('\n');

  mvr::MvrImportResourceResolver resolver(root);
  assert(resolver.ResolveGdtfPath(exactName) == PathUtils::PathToUtf8(exact));
  assert(resolver.ResolveGdtfPath(
             PathUtils::PathToUtf8(fs::path(u8"Fixture Ä"))) ==
         PathUtils::PathToUtf8(exact));
  assert(resolver.ResolveGdtfPath(
             PathUtils::PathToUtf8(fs::path(u8"fixture Ä.GDTF"))) ==
         PathUtils::PathToUtf8(exact));
  assert(resolver.ResolveGdtfPath("Canonical Fixture") ==
         PathUtils::PathToUtf8(canonical));
  assert(resolver.ResolveGdtfPath("---.gdtf") !=
         PathUtils::PathToUtf8(unrelated));
  assert(!resolver.GdtfFileExists(resolver.ResolveGdtfPath("---.gdtf")));
  const std::string &first = resolver.ResolveGdtfPath(exactName);
  const std::string &second = resolver.ResolveGdtfPath(exactName);
  assert(&first == &second);
  assert(!resolver.GdtfFileExists(resolver.ResolveGdtfPath("missing")));
  assert(resolver.MakeSceneRelative(exact) == exactName);
  const fs::path unicodeModelPath = fs::path(u8"models/é.glb");
  assert(resolver.ResolveScenePath(PathUtils::PathToUtf8(unicodeModelPath)) ==
         root / unicodeModelPath);
  assert(!resolver.GdtfFileExists(
      resolver.ResolveGdtfPath("Missing Fixture .gdtf")));
  assert(resolver.NormalizeSupportGdtfSpec("Missing Fixture .gdtf") ==
         "Missing Fixture .gdtf");
  assert(resolver.NormalizeSupportGdtfSpec("Support Fixture .gdtf") ==
         "Support Fixture.gdtf");

  const std::vector<std::string> modes = {"Mode 16", "Standard", "Basic"};
  const auto count = [](const std::string &mode) {
    return mode == "Basic" ? 8 : 16;
  };
  assert(mvr::MvrImportResourceResolver::SelectMode(modes, " mode 16 ", {},
                                                    count) == "Mode 16");
  assert(mvr::MvrImportResourceResolver::SelectMode(modes, "Legacy 016", {},
                                                    count) == "Mode 16");
  assert(mvr::MvrImportResourceResolver::SelectMode(modes, "Unknown", 8,
                                                    count) == "Basic");
  assert(mvr::MvrImportResourceResolver::SelectMode(modes, "Unknown", {},
                                                    count) == "Standard");
  assert(mvr::MvrImportResourceResolver::SelectMode(
             {"First", "Second"}, "Unknown", {}, count) == "First");

  resolver.DictionaryEntry("Fixture");
  resolver.DictionaryEntry("Fixture");
  assert(dictionaryLookups == 1);
  fs::remove_all(root);
  return 0;
}
