#include "active_dictionary_storage.h"
#include "filesystem_path_utils.h"
#include "json.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// Writes deterministic test content to a file.
static void WriteText(const fs::path &path, const std::string &text) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out << text;
}

// Verifies default and custom dictionary storage layouts.
static void TestStorageLayouts(const fs::path &root) {
  const fs::path defaultFixture = root / "fixtures" / "gdtf_dictionary.json";
  const fs::path defaultTruss = root / "trusses" / "truss_dictionary.json";
  const fs::path customFixture = root / "shows" / "MyTour.json";
  const fs::path customTruss = root / "shows" / "RigTour.json";

  auto fixtureDefault = ActiveDictionaryStorage::BuildLayout(
      ActiveDictionaryStorage::DictionaryKind::Fixtures, defaultFixture,
      defaultFixture);
  assert(fixtureDefault.isDefaultManagedDictionary);
  assert(fixtureDefault.ownedAssetDirectory == defaultFixture.parent_path());

  auto trussDefault = ActiveDictionaryStorage::BuildLayout(
      ActiveDictionaryStorage::DictionaryKind::Trusses, defaultTruss,
      defaultTruss);
  assert(trussDefault.isDefaultManagedDictionary);
  assert(trussDefault.ownedAssetDirectory == defaultTruss.parent_path());

  auto fixtureCustom = ActiveDictionaryStorage::BuildLayout(
      ActiveDictionaryStorage::DictionaryKind::Fixtures, customFixture,
      defaultFixture);
  assert(!fixtureCustom.isDefaultManagedDictionary);
  assert(fixtureCustom.ownedAssetDirectory == root / "shows" / "MyTour_assets");

  auto trussCustom = ActiveDictionaryStorage::BuildLayout(
      ActiveDictionaryStorage::DictionaryKind::Trusses, customTruss,
      defaultTruss);
  assert(!trussCustom.isDefaultManagedDictionary);
  assert(trussCustom.ownedAssetDirectory == root / "shows" / "RigTour_assets");
}

// Verifies supported legacy and owned reference resolution forms.
static void TestReferenceResolution(const fs::path &root) {
  const fs::path dict = root / "external" / "Tour.json";
  const fs::path beside = dict.parent_path() / "beside.gdtf";
  const fs::path asset = dict.parent_path() / "Tour_assets" / "owned.gdtf";
  const fs::path absolute = root / "absolute.gdtf";
  WriteText(beside, "beside");
  WriteText(asset, "owned");
  WriteText(absolute, "absolute");

  assert(ActiveDictionaryStorage::ResolveReference(dict, "beside.gdtf") == beside);
  assert(ActiveDictionaryStorage::ResolveReference(dict, "owned.gdtf") == asset);
  assert(ActiveDictionaryStorage::ResolveReference(
             dict, PathUtils::PathToUtf8(absolute)) == absolute);
}

// Verifies Unicode dictionary layouts and JSON-safe portable asset references.
static void TestUnicodeReferenceRoundtrip(const fs::path &root) {
  const fs::path defaultDict = root / "fixtures" / "gdtf_dictionary.json";
  const fs::path dictionary =
      root / PathUtils::PathFromUtf8("shows/diccionario_niño.json");
  const fs::path expectedAssetDirectory =
      root / PathUtils::PathFromUtf8("shows/diccionario_niño_assets");
  const auto layout = ActiveDictionaryStorage::BuildLayout(
      ActiveDictionaryStorage::DictionaryKind::Fixtures, dictionary, defaultDict);
  assert(layout.ownedAssetDirectory == expectedAssetDirectory);

  const fs::path ownedAsset =
      expectedAssetDirectory / PathUtils::PathFromUtf8("cabeza_móvil_á.gdtf");
  WriteText(ownedAsset, "owned");
  const std::string ownedReference =
      ActiveDictionaryStorage::MakeSerializedReference(layout, ownedAsset);
  assert(ownedReference == "cabeza_móvil_á.gdtf");
  assert(PathUtils::PathFromUtf8(ownedReference).is_relative());
  assert(ActiveDictionaryStorage::ResolveReference(dictionary, ownedReference) ==
         ownedAsset);

  const fs::path externalAsset =
      fs::absolute(root / PathUtils::PathFromUtf8("externo/estructura_ñ.gdtf"));
  WriteText(externalAsset, "external");
  const std::string externalReference =
      ActiveDictionaryStorage::MakeSerializedReference(layout, externalAsset);
  assert(externalReference == PathUtils::PathToUtf8(externalAsset));
  assert(ActiveDictionaryStorage::ResolveReference(dictionary, externalReference) ==
         externalAsset);

  nlohmann::json serialized = {
      {"owned", ownedReference}, {"external", externalReference}};
  const std::string dumped = serialized.dump();
  assert(dumped.find(ownedReference) != std::string::npos);
  assert(dumped.find(externalReference) != std::string::npos);
}

// Verifies deterministic same-name conflict handling for copied assets.
static void TestConflictHandling(const fs::path &root) {
  const fs::path defaultDict = root / "fixtures" / "gdtf_dictionary.json";
  const fs::path customDict = root / "shows" / "Tour.json";
  const fs::path first = root / "src1" / "fixture.gdtf";
  const fs::path second = root / "src2" / "fixture.gdtf";
  WriteText(first, "first");
  WriteText(second, "second");

  auto copiedFirst = ActiveDictionaryStorage::CopyAssetIntoDictionaryStorage(
      {ActiveDictionaryStorage::DictionaryKind::Fixtures, customDict, defaultDict,
       first, {}, FileImportUtils::ConflictPolicy::Rename});
  assert(copiedFirst.success);
  assert(copiedFirst.finalPath == root / "shows" / "Tour_assets" / "fixture.gdtf");
  assert(copiedFirst.serializedPath == "fixture.gdtf");

  auto copiedSecond = ActiveDictionaryStorage::CopyAssetIntoDictionaryStorage(
      {ActiveDictionaryStorage::DictionaryKind::Fixtures, customDict, defaultDict,
       second, {}, FileImportUtils::ConflictPolicy::Rename});
  assert(copiedSecond.success);
  assert(copiedSecond.finalPath != copiedFirst.finalPath);
  assert(copiedSecond.finalPath.filename() != copiedFirst.finalPath.filename());
}

int main() {
  const fs::path root = fs::temp_directory_path() / "perastage_active_dictionary_storage_test";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root);

  TestStorageLayouts(root);
  TestReferenceResolution(root);
  TestUnicodeReferenceRoundtrip(root);
  TestConflictHandling(root);

  fs::remove_all(root, ec);
  return 0;
}
