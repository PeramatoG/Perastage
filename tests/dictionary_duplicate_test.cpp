#include "dictionary_duplicate.h"
#include "dictionary_json_contract.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

// Writes text content to a test file.
static void WriteText(const fs::path &path, const std::string &text) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path);
  out << text;
}

// Reads a JSON document from a test file.
static nlohmann::json ReadJson(const fs::path &path) {
  std::ifstream in(path);
  nlohmann::json root;
  in >> root;
  return root;
}

// Creates a clean temporary root for dictionary duplication tests.
static fs::path ResetRoot() {
  const fs::path root =
      fs::temp_directory_path() / "perastage_dictionary_duplicate_test";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root);
  return root;
}

// Verifies fixture dictionary duplication copies assets and metadata
// independently.
static void TestDuplicateFixturesCopiesAssetsAndMetadata() {
  const fs::path root = ResetRoot();
  const fs::path source = root / "source" / "fixtures.json";
  const fs::path assets = root / "source" / "fixtures_assets";
  WriteText(assets / "a.gdtf", "fixture-a");
  WriteText(assets / "b.gdtf", "fixture-b");
  nlohmann::json entries = nlohmann::json::object();
  entries["A"] = {{"file", "a.gdtf"},
                  {"mode", "Mode 1"},
                  {"category", "Wash"},
                  {"visual_color", "#112233"},
                  {"imported_at", "2026-01-01T00:00:00Z"},
                  {"sha256", "abc"}};
  entries["B"] = {{"file", "b.gdtf"},
                  {"mode", "Mode 2"},
                  {"category", "Spot"},
                  {"visual_color", "#445566"}};
  WriteText(source,
            DictionaryJsonContract::MakeRoot("fixtures", entries).dump(4));

  const fs::path dest = root / "duplicate" / "fixtures_copy.json";
  const auto result = DictionaryDuplicate::DuplicateDictionary(
      {ActiveDictionaryStorage::DictionaryKind::Fixtures, source,
       root / "default" / "gdtf_dictionary.json", dest});
  assert(result.success);
  assert(result.copiedAssetCount == 2);
  assert(fs::exists(root / "duplicate" / "fixtures_copy_assets" / "a.gdtf"));
  assert(fs::exists(root / "duplicate" / "fixtures_copy_assets" / "b.gdtf"));

  auto duplicated = ReadJson(dest);
  assert(duplicated["entries"]["A"]["file"] == "a.gdtf");
  assert(duplicated["entries"]["A"]["mode"] == "Mode 1");
  assert(duplicated["entries"]["A"]["category"] == "Wash");
  assert(duplicated["entries"]["A"]["visual_color"] == "#112233");
  assert(duplicated["entries"]["A"]["imported_at"] == "2026-01-01T00:00:00Z");
  assert(duplicated["entries"]["A"]["sha256"] == "abc");

  fs::remove_all(source.parent_path());
  std::string error;
  assert(
      DictionaryJsonContract::GetEntriesForType(duplicated, "fixtures", error));
  assert(fs::exists(root / "duplicate" / "fixtures_copy_assets" / "a.gdtf"));
}

// Verifies truss dictionary duplication copies multiple files and preserves
// metadata.
static void TestDuplicateTrussesCopiesAssetsAndMetadata() {
  const fs::path root = ResetRoot();
  const fs::path source = root / "source" / "trusses.json";
  const fs::path assets = root / "source" / "trusses_assets";
  WriteText(assets / "box.gdtf", "box");
  WriteText(assets / "tri.gdtf", "tri");
  nlohmann::json entries = nlohmann::json::object();
  entries["box truss"] = {
      {"file", "box.gdtf"}, {"imported_at", "old"}, {"sha256", "111"}};
  entries["tri truss"] = {{"file", "tri.gdtf"}, {"sha256", "222"}};
  WriteText(source,
            DictionaryJsonContract::MakeRoot("trusses", entries).dump(4));

  const fs::path dest = root / "duplicate" / "trusses_copy.json";
  const auto result = DictionaryDuplicate::DuplicateDictionary(
      {ActiveDictionaryStorage::DictionaryKind::Trusses, source,
       root / "default" / "truss_dictionary.json", dest});
  assert(result.success);
  assert(result.copiedAssetCount == 2);
  assert(fs::exists(root / "duplicate" / "trusses_copy_assets" / "box.gdtf"));
  assert(fs::exists(root / "duplicate" / "trusses_copy_assets" / "tri.gdtf"));
  auto duplicated = ReadJson(dest);
  assert(duplicated["entries"]["box truss"]["file"] == "box.gdtf");
  assert(duplicated["entries"]["box truss"]["imported_at"] == "old");
  fs::remove_all(source.parent_path());
  assert(fs::exists(root / "duplicate" / "trusses_copy_assets" / "tri.gdtf"));
}

// Verifies unresolved references are preserved and reported.
static void TestUnresolvedReferencesArePreserved() {
  const fs::path root = ResetRoot();
  const fs::path source = root / "source" / "fixtures.json";
  nlohmann::json entries = nlohmann::json::object();
  entries["Missing"] = {{"file", "missing.gdtf"}, {"mode", "Mode"}};
  WriteText(source,
            DictionaryJsonContract::MakeRoot("fixtures", entries).dump(4));

  const fs::path dest = root / "duplicate" / "fixtures_copy.json";
  const auto result = DictionaryDuplicate::DuplicateDictionary(
      {ActiveDictionaryStorage::DictionaryKind::Fixtures, source,
       root / "default" / "gdtf_dictionary.json", dest});
  if (!result.success) {
    for (const auto &error : result.errors)
      std::cerr << error << "\n";
  }
  assert(result.success);
  assert(result.unresolvedReferenceCount == 1);
  auto duplicated = ReadJson(dest);
  assert(duplicated["entries"]["Missing"]["file"] == "missing.gdtf");
}

// Verifies equal source and destination paths are rejected without output
// mutation.
static void TestSameDestinationRejected() {
  const fs::path root = ResetRoot();
  const fs::path source = root / "fixtures.json";
  WriteText(source, DictionaryJsonContract::MakeRoot("fixtures",
                                                     nlohmann::json::object())
                        .dump(4));
  const auto result = DictionaryDuplicate::DuplicateDictionary(
      {ActiveDictionaryStorage::DictionaryKind::Fixtures, source,
       root / "gdtf_dictionary.json", source});
  assert(!result.success);
  assert(!result.errors.empty());
}

// Verifies overwrite creates a backup and installs replacement assets.
static void TestOverwriteCreatesBackup() {
  const fs::path root = ResetRoot();
  const fs::path source = root / "source" / "fixtures.json";
  WriteText(root / "source" / "fixtures_assets" / "a.gdtf", "new");
  nlohmann::json entries = nlohmann::json::object();
  entries["A"] = {{"file", "a.gdtf"}};
  WriteText(source,
            DictionaryJsonContract::MakeRoot("fixtures", entries).dump(4));
  const fs::path dest = root / "dest" / "copy.json";
  WriteText(dest, "old json");
  WriteText(root / "dest" / "copy_assets" / "old.gdtf", "old");

  const auto result = DictionaryDuplicate::DuplicateDictionary(
      {ActiveDictionaryStorage::DictionaryKind::Fixtures, source,
       root / "default" / "gdtf_dictionary.json", dest});
  assert(result.success);
  assert(!result.backupDictionaryPath.empty());
  assert(!result.backupAssetsPath.empty());
  assert(fs::exists(result.backupDictionaryPath));
  assert(fs::exists(result.backupAssetsPath / "old.gdtf"));
  assert(fs::exists(root / "dest" / "copy_assets" / "a.gdtf"));
}

// Runs all dictionary duplication service regression tests.
int main() {
  TestDuplicateFixturesCopiesAssetsAndMetadata();
  TestDuplicateTrussesCopiesAssetsAndMetadata();
  TestUnresolvedReferencesArePreserved();
  TestSameDestinationRejected();
  TestOverwriteCreatesBackup();
  std::cout << "dictionary duplicate tests passed\n";
  return 0;
}
