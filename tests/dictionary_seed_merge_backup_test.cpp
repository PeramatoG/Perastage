/*
 * This file is part of Perastage.
 */
#include <cassert>
#include "filesystem_path_utils.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include <wx/init.h>

#include "configmanager.h"
#include "dictionary_json_contract.h"
#include "gdtfdictionary.h"
#include "json.hpp"
#include "projectutils.h"
#include "trussdictionary.h"

namespace fs = std::filesystem;

namespace {

std::string ReadFile(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);
  assert(in.is_open());
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
}

void WriteFile(const fs::path &path, const std::string &content) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  assert(out.is_open());
  out << content;
  assert(out.good());
}

void SetLibraryPathEnv(const std::string &value) {
#ifdef _WIN32
  _putenv_s("PERASTAGE_LIBRARY_PATH", value.c_str());
#else
  setenv("PERASTAGE_LIBRARY_PATH", value.c_str(), 1);
#endif
}

std::string FirstEntryKeyFromBase(const fs::path &baseDictionaryPath,
                                  const std::string &dictionaryType) {
  nlohmann::json root;
  {
    std::ifstream in(baseDictionaryPath);
    assert(in.is_open());
    in >> root;
  }
  std::string error;
  auto entriesOpt = DictionaryJsonContract::GetEntriesForType(root, dictionaryType, error);
  assert(entriesOpt.has_value());
  const nlohmann::json &entries = **entriesOpt;
  assert(entries.is_object());
  assert(!entries.empty());
  return entries.begin().key();
}

void VerifyFixtureLoadIsReadOnly(const fs::path &root) {
  const fs::path userPath = root / "fixtures" / "gdtf_dictionary.json";
  nlohmann::json entries = nlohmann::json::object();
  entries["CUSTOM FIXTURE KEEP"] = {{"color", "#112233"}};
  WriteFile(userPath, DictionaryJsonContract::MakeRoot("fixtures", entries).dump(2));
  const std::string before = ReadFile(userPath);
  const auto timeBefore = fs::last_write_time(userPath);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  const fs::path basePath =
      ProjectUtils::GetBaseLibraryPath("fixtures") / "gdtf_dictionary.json";
  const std::string seedKey = FirstEntryKeyFromBase(basePath, "fixtures");
  assert(seedKey != "CUSTOM FIXTURE KEEP");
  GdtfDictionary::ResetSaveCallCountForTesting();

  auto loadedOpt = GdtfDictionary::Load();
  assert(loadedOpt.has_value());
  assert(loadedOpt->find("CUSTOM FIXTURE KEEP") != loadedOpt->end());
  assert(loadedOpt->find(seedKey) == loadedOpt->end());
  assert(ReadFile(userPath) == before);
  assert(fs::last_write_time(userPath) == timeBefore);
  assert(!fs::exists(userPath.string() + ".bak"));
  assert(GdtfDictionary::GetSaveCallCountForTesting() == 0);
}

void VerifyTrussLoadIsReadOnly(const fs::path &root) {
  const fs::path userPath = root / "trusses" / "truss_dictionary.json";
  const fs::path customAsset = userPath.parent_path() / "custom.gdtf";
  WriteFile(customAsset, "custom");
  nlohmann::json entries = nlohmann::json::object();
  entries["CUSTOM TRUSS KEEP"] = {{"file", customAsset.filename().string()}};
  WriteFile(userPath, DictionaryJsonContract::MakeRoot("trusses", entries).dump(2));
  const std::string before = ReadFile(userPath);
  const auto timeBefore = fs::last_write_time(userPath);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  const fs::path basePath =
      ProjectUtils::GetBaseLibraryPath("trusses") / "truss_dictionary.json";
  const std::string seedKey = FirstEntryKeyFromBase(basePath, "trusses");
  assert(seedKey != "CUSTOM TRUSS KEEP");

  auto loadedOpt = TrussDictionary::Load();
  assert(loadedOpt.has_value());
  assert(loadedOpt->find(TrussDictionary::NormalizeModelKey("CUSTOM TRUSS KEEP")) != loadedOpt->end());
  assert(loadedOpt->find(TrussDictionary::NormalizeModelKey(seedKey)) == loadedOpt->end());
  assert(ReadFile(userPath) == before);
  assert(fs::last_write_time(userPath) == timeBefore);
  assert(!fs::exists(userPath.string() + ".bak"));
}

void VerifyManagedDefaultRecoveryCreatesBackup(const fs::path &root) {
  const fs::path userPath = root / "fixtures" / "gdtf_dictionary.json";
  WriteFile(userPath, "{ invalid json");
  assert(GdtfDictionary::Load().has_value());
  const auto status = GdtfDictionary::GetLastLoadStatus();
  assert(status.managedDefaultRecreated);
  assert(status.activeDictionaryInvalid);
  assert(fs::exists(userPath.string() + ".bak"));
}

void VerifyInvalidCustomDictionaryIsPreserved(const fs::path &root) {
  const fs::path customPath = root / "custom" / "bad_gdtf_dictionary.json";
  WriteFile(customPath, "{ invalid json");
  ConfigManager::Get().SetValue("fixtures_dictionary_active_path",
                                customPath.string());
  const std::string before = ReadFile(customPath);
  assert(GdtfDictionary::Load().has_value());
  const auto status = GdtfDictionary::GetLastLoadStatus();
  assert(status.activeDictionaryInvalid);
  assert(status.temporaryFallbackUsed);
  assert(!status.managedDefaultRecreated);
  assert(ReadFile(customPath) == before);
  assert(!fs::exists(customPath.string() + ".bak"));
  ConfigManager::Get().RemoveKey("fixtures_dictionary_active_path");
}

} // namespace

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const fs::path tempRoot = fs::temp_directory_path() /
                            PathUtils::PathFromUtf8("perastage_dictionary_seed_merge_backup_test");
  std::error_code ec;
  fs::remove_all(tempRoot, ec);
  fs::create_directories(tempRoot);

  ConfigManager::Get().Reset();
  SetLibraryPathEnv(tempRoot.string());
  VerifyFixtureLoadIsReadOnly(tempRoot);
  VerifyTrussLoadIsReadOnly(tempRoot);
  VerifyManagedDefaultRecoveryCreatesBackup(tempRoot);
  VerifyInvalidCustomDictionaryIsPreserved(tempRoot);

  fs::remove_all(tempRoot, ec);
  return 0;
}
