/*
 * This file is part of Perastage.
 */
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <wx/init.h>

#include "configmanager.h"
#include "dictionary_json_contract.h"
#include "filesystem_path_utils.h"
#include "gdtfdictionary.h"
#include "json.hpp"
#include "projectutils.h"
#include "trussdictionary.h"

namespace fs = std::filesystem;

namespace {

// Writes UTF-8 text to a file for test setup.
void WriteFile(const fs::path &path, const std::string &content) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  assert(out.is_open());
  out << content;
  assert(out.good());
}

// Sets the writable library root used by dictionary code.
void SetLibraryPathEnv(const std::string &value) {
#ifdef _WIN32
  _putenv_s("PERASTAGE_LIBRARY_PATH", value.c_str());
#else
  setenv("PERASTAGE_LIBRARY_PATH", value.c_str(), 1);
#endif
}

// Creates a contract-compliant fixture dictionary.
void WriteFixtureDictionary(const fs::path &path) {
  nlohmann::json entries = nlohmann::json::object();
  entries["Fixture A"] = {{"file", "fixture.gdtf"}, {"mode", "Mode 1"}};
  WriteFile(path, DictionaryJsonContract::MakeRoot("fixtures", entries).dump(2));
}

// Creates a contract-compliant truss dictionary.
void WriteTrussDictionary(const fs::path &path) {
  nlohmann::json entries = nlohmann::json::object();
  entries["Truss A"] = {{"file", "truss.gdtf"}};
  WriteFile(path, DictionaryJsonContract::MakeRoot("trusses", entries).dump(2));
}

// Verifies explicit open, new, and default active dictionary workflows.
void VerifyActiveDictionaryWorkflows(const fs::path &root) {
  ConfigManager::Get().Reset();
  SetLibraryPathEnv(root.string());

  const fs::path defaultFixture = root / "fixtures" / "gdtf_dictionary.json";
  const fs::path defaultTruss = root / "trusses" / "truss_dictionary.json";
  WriteFixtureDictionary(defaultFixture);
  WriteTrussDictionary(defaultTruss);

  const fs::path fixturePath = root / "custom" / "fixture_custom.json";
  const fs::path trussPath = root / "custom" / "truss_custom.json";
  WriteFixtureDictionary(fixturePath);
  WriteTrussDictionary(trussPath);

  std::string error;
  assert(GdtfDictionary::SetActiveDictionaryFilePath(fixturePath.string(), &error));
  assert(GdtfDictionary::GetActiveDictionaryFilePath() == fixturePath.string());
  assert(!GdtfDictionary::SetActiveDictionaryFilePath(trussPath.string(), &error));
  assert(GdtfDictionary::GetActiveDictionaryFilePath() == fixturePath.string());

  assert(TrussDictionary::SetActiveDictionaryFilePath(trussPath.string(), &error));
  assert(TrussDictionary::GetActiveDictionaryFilePath() == trussPath.string());
  assert(!TrussDictionary::SetActiveDictionaryFilePath(fixturePath.string(), &error));
  assert(TrussDictionary::GetActiveDictionaryFilePath() == trussPath.string());

  const fs::path missingPath = root / "custom" / "missing.json";
  assert(!GdtfDictionary::SetActiveDictionaryFilePath(missingPath.string(), &error));
  assert(!fs::exists(missingPath));
  assert(GdtfDictionary::GetActiveDictionaryFilePath() == fixturePath.string());

  const fs::path emptyFixture = root / "custom" / "empty_fixture.json";
  const fs::path emptyTruss = root / "custom" / "empty_truss.json";
  assert(GdtfDictionary::CreateEmptyDictionaryFile(emptyFixture.string(), &error));
  assert(GdtfDictionary::ValidateDictionaryFile(emptyFixture.string(), &error));
  assert(TrussDictionary::CreateEmptyDictionaryFile(emptyTruss.string(), &error));
  assert(TrussDictionary::ValidateDictionaryFile(emptyTruss.string(), &error));

  const fs::path defaultsFixture = root / "custom" / "defaults_fixture.json";
  const fs::path defaultsTruss = root / "custom" / "defaults_truss.json";
  assert(GdtfDictionary::CreateDictionaryFileFromDefaults(defaultsFixture.string(), &error));
  assert(GdtfDictionary::ValidateDictionaryFile(defaultsFixture.string(), &error));
  assert(TrussDictionary::CreateDictionaryFileFromDefaults(defaultsTruss.string(), &error));
  assert(TrussDictionary::ValidateDictionaryFile(defaultsTruss.string(), &error));

  const fs::path invalidNew = root / "custom" / "invalid_new.json";
  WriteFile(invalidNew, "not json");
  assert(!GdtfDictionary::ValidateDictionaryFile(invalidNew.string(), &error));
  assert(GdtfDictionary::GetActiveDictionaryFilePath() == fixturePath.string());

  assert(GdtfDictionary::SetActiveDictionaryFilePath({}, &error));
  assert(GdtfDictionary::GetActiveDictionaryFilePath() == defaultFixture.string());
  assert(TrussDictionary::SetActiveDictionaryFilePath({}, &error));
  assert(TrussDictionary::GetActiveDictionaryFilePath() == defaultTruss.string());
}

} // namespace

// Runs active dictionary workflow regression checks.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const fs::path tempRoot = fs::temp_directory_path() /
                            PathUtils::PathFromUtf8("perastage_active_dictionary_workflow_test");
  std::error_code ec;
  fs::remove_all(tempRoot, ec);
  VerifyActiveDictionaryWorkflows(tempRoot);
  fs::remove_all(tempRoot, ec);
  return 0;
}
