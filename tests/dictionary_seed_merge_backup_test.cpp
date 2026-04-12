/*
 * This file is part of Perastage.
 */
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <wx/init.h>

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

void VerifyFixturesMergeAndBackup(const fs::path &writableRoot) {
  const fs::path writableFixturesDir = writableRoot / "fixtures";
  fs::create_directories(writableFixturesDir);
  const fs::path userPath = writableFixturesDir / "gdtf_dictionary.json";

  nlohmann::json userEntries = nlohmann::json::object();
  userEntries["CUSTOM FIXTURE KEEP"] = { {"color", "#112233"} };
  WriteFile(userPath,
            DictionaryJsonContract::MakeRoot("fixtures", std::move(userEntries)).dump(2));
  const std::string before = ReadFile(userPath);

  const fs::path basePath =
      ProjectUtils::GetBaseLibraryPath("fixtures") / "gdtf_dictionary.json";
  const std::string seedKey = FirstEntryKeyFromBase(basePath, "fixtures");
  assert(seedKey != "CUSTOM FIXTURE KEEP");

  auto loadedOpt = GdtfDictionary::Load();
  assert(loadedOpt.has_value());
  const auto &loaded = *loadedOpt;
  assert(loaded.find("CUSTOM FIXTURE KEEP") != loaded.end());
  assert(loaded.find(seedKey) != loaded.end());

  const fs::path backupPath = userPath.string() + ".bak";
  assert(fs::exists(backupPath));
  const std::string backupText = ReadFile(backupPath);
  assert(backupText == before);
}

void VerifyTrussesMergeAndBackup(const fs::path &writableRoot) {
  const fs::path writableTrussesDir = writableRoot / "trusses";
  fs::create_directories(writableTrussesDir);
  const fs::path userPath = writableTrussesDir / "truss_dictionary.json";

  const fs::path customAsset = writableTrussesDir / "custom.gdtf";
  WriteFile(customAsset, "custom");

  nlohmann::json userEntries = nlohmann::json::object();
  userEntries["CUSTOM TRUSS KEEP"] = { {"file", customAsset.filename().string()} };
  WriteFile(userPath,
            DictionaryJsonContract::MakeRoot("trusses", std::move(userEntries)).dump(2));
  const std::string before = ReadFile(userPath);

  const fs::path basePath =
      ProjectUtils::GetBaseLibraryPath("trusses") / "truss_dictionary.json";
  const std::string seedKey = FirstEntryKeyFromBase(basePath, "trusses");
  assert(seedKey != "CUSTOM TRUSS KEEP");

  auto loadedOpt = TrussDictionary::Load();
  assert(loadedOpt.has_value());
  const auto &loaded = *loadedOpt;
  assert(loaded.find(TrussDictionary::NormalizeModelKey("CUSTOM TRUSS KEEP")) != loaded.end());
  assert(loaded.find(TrussDictionary::NormalizeModelKey(seedKey)) != loaded.end());

  const fs::path backupPath = userPath.string() + ".bak";
  assert(fs::exists(backupPath));
  const std::string backupText = ReadFile(backupPath);
  assert(backupText == before);
}

} // namespace

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const fs::path tempRoot = fs::temp_directory_path() /
                            fs::u8path("perastage_dictionary_seed_merge_backup_test");
  std::error_code ec;
  fs::remove_all(tempRoot, ec);
  fs::create_directories(tempRoot);

  SetLibraryPathEnv(tempRoot.string());
  VerifyFixturesMergeAndBackup(tempRoot);
  VerifyTrussesMergeAndBackup(tempRoot);

  fs::remove_all(tempRoot, ec);
  return 0;
}
