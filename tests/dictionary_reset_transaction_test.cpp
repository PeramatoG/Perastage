#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <wx/init.h>

#include "configmanager.h"
#include "dictionary_json_contract.h"
#include "dictionary_reset_service.h"
#include "filesystem_path_utils.h"
#include "gdtfdictionary.h"
#include "json.hpp"
#include "trussdictionary.h"

namespace fs = std::filesystem;

namespace {

// Writes UTF-8 text to a file for test setup.
void WriteFile(const fs::path &path, const std::string &content) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  assert(out.is_open());
  out << content;
}

// Reads a JSON file from disk for assertions.
nlohmann::json ReadJson(const fs::path &path) {
  std::ifstream in(path);
  assert(in.is_open());
  nlohmann::json root;
  in >> root;
  return root;
}

// Sets the writable library root used by dictionary code.
void SetLibraryPathEnv(const std::string &value) {
#ifdef _WIN32
  _putenv_s("PERASTAGE_LIBRARY_PATH", value.c_str());
#else
  setenv("PERASTAGE_LIBRARY_PATH", value.c_str(), 1);
#endif
}

// Verifies fixture reset accepts file and legacy path fields and writes canonical file fields.
void VerifyFixtureResetCanonicalizesFileFields(const fs::path &root) {
  ConfigManager::Get().Reset();
  SetLibraryPathEnv(root.string());

  const fs::path appJson = root / "app" / "fixtures" / "gdtf_dictionary.json";
  WriteFile(appJson.parent_path() / "spot.gdtf", "spot");
  WriteFile(appJson.parent_path() / "wash.gdtf", "wash");

  nlohmann::json entries = nlohmann::json::object();
  entries["Spot"] = {{"file", "spot.gdtf"},
                     {"mode", "Basic"},
                     {"category", "Profile"}};
  entries["Wash"] = {{"path", "wash.gdtf"},
                     {"mode", "Wide"},
                     {"visual_color", "#ffffff"}};
  WriteFile(appJson, DictionaryJsonContract::MakeRoot("fixtures", entries).dump(2));

  const fs::path activeJson = root / "show" / "fixture_custom.json";
  WriteFile(activeJson,
            DictionaryJsonContract::MakeRoot("fixtures", nlohmann::json::object()).dump(2));
  std::string activeError;
  assert(GdtfDictionary::SetActiveDictionaryFilePath(activeJson.string(), &activeError));

  const auto summary = DictionaryResetService::ResetToDefaults(
      {ActiveDictionaryStorage::DictionaryKind::Fixtures, activeJson,
       root / "fixtures" / "gdtf_dictionary.json", appJson});
  assert(!summary.HasErrors());

  const auto resultEntries = ReadJson(activeJson)["entries"];
  assert(resultEntries.size() == 2);
  assert(resultEntries["Spot"].contains("file"));
  assert(!resultEntries["Wash"].contains("path"));
  assert(resultEntries["Spot"]["mode"] == "Basic");
  assert(resultEntries["Wash"]["visual_color"] == "#ffffff");
  assert(fs::exists(activeJson.parent_path() / "fixture_custom_assets" / "spot.gdtf"));
}

// Verifies reset refuses missing defaults without replacing active JSON.
void VerifyInvalidResetDoesNotReplaceActiveJson(const fs::path &root) {
  const fs::path appJson = root / "app_invalid" / "fixtures" / "gdtf_dictionary.json";
  nlohmann::json entries = nlohmann::json::object();
  entries["Broken"] = {{"file", "missing.gdtf"}};
  WriteFile(appJson, DictionaryJsonContract::MakeRoot("fixtures", entries).dump(2));

  const fs::path activeJson = root / "show" / "protected_fixture.json";
  WriteFile(activeJson.parent_path() / "existing.gdtf", "existing");
  nlohmann::json activeEntries = nlohmann::json::object();
  activeEntries["Existing"] = {{"file", "existing.gdtf"}};
  WriteFile(activeJson,
            DictionaryJsonContract::MakeRoot("fixtures", activeEntries).dump(2));
  std::string activeError;
  assert(GdtfDictionary::SetActiveDictionaryFilePath(activeJson.string(), &activeError));

  const auto before = ReadJson(activeJson).dump();
  const auto summary = DictionaryResetService::ResetToDefaults(
      {ActiveDictionaryStorage::DictionaryKind::Fixtures, activeJson,
       root / "fixtures" / "gdtf_dictionary.json", appJson});

  assert(summary.HasErrors());
  assert(ReadJson(activeJson).dump() == before);
}

// Verifies managed default reset replaces existing assets without rename-over-existing assumptions.
void VerifyManagedResetReplacesExistingAsset(const fs::path &root) {
  const fs::path appJson = root / "app_truss" / "trusses" / "truss_dictionary.json";
  WriteFile(appJson.parent_path() / "box.gdtf", "new");
  nlohmann::json entries = nlohmann::json::object();
  entries["Box"] = {{"file", "box.gdtf"}};
  WriteFile(appJson, DictionaryJsonContract::MakeRoot("trusses", entries).dump(2));

  const fs::path activeJson = root / "trusses" / "truss_dictionary.json";
  WriteFile(activeJson.parent_path() / "box.gdtf", "old");
  WriteFile(activeJson,
            DictionaryJsonContract::MakeRoot("trusses", nlohmann::json::object()).dump(2));
  std::string activeError;
  assert(TrussDictionary::SetActiveDictionaryFilePath(activeJson.string(), &activeError));

  const auto summary = DictionaryResetService::ResetToDefaults(
      {ActiveDictionaryStorage::DictionaryKind::Trusses, activeJson, activeJson, appJson});
  assert(!summary.HasErrors());

  std::ifstream in(activeJson.parent_path() / "box.gdtf", std::ios::binary);
  std::string content;
  in >> content;
  assert(content == "new");
}

} // namespace

// Runs dictionary reset transaction regression checks.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const fs::path root = fs::temp_directory_path() /
                        "perastage_dictionary_reset_transaction_test";
  fs::remove_all(root);
  VerifyFixtureResetCanonicalizesFileFields(root);
  VerifyInvalidResetDoesNotReplaceActiveJson(root);
  VerifyManagedResetReplacesExistingAsset(root);
  fs::remove_all(root);
}
