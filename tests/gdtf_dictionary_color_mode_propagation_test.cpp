/*
 * This file is part of Perastage.
 */
#include "filesystem_path_utils.h"
#include <cassert>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <wx/init.h>

#include "configmanager.h"
#include "dictionary_json_contract.h"
#include "gdtfdictionary.h"
#include "json.hpp"
#include "projectutils.h"
#include "support/gdtf_test_fixture_builder.h"

namespace {

std::string ReadFile(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

void WriteFile(const std::filesystem::path &path, const std::string &content) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  assert(out.is_open());
  out << content;
  assert(out.good());
}

// Points the test at an isolated writable library root.
void SetLibraryPathEnv(const std::filesystem::path &value) {
#ifdef _WIN32
  _putenv_s("PERASTAGE_LIBRARY_PATH", value.string().c_str());
#else
  setenv("PERASTAGE_LIBRARY_PATH", value.string().c_str(), 1);
#endif
}

// Returns a unique temporary root for dictionary isolation.
std::filesystem::path MakeIsolatedRoot(const char *name) {
  return std::filesystem::temp_directory_path() /
         (std::string(name) + "_" +
          std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
}

} // namespace

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const std::filesystem::path isolatedRoot = MakeIsolatedRoot("perastage_dictionary_gdtf_test");
  SetLibraryPathEnv(isolatedRoot);
  const std::filesystem::path fixturesDir = isolatedRoot / "fixtures";
  std::filesystem::create_directories(fixturesDir);
  const std::filesystem::path dictPath = fixturesDir / "gdtf_dictionary.json";
  ConfigManager::Get().SetValue("fixtures_dictionary_active_path", dictPath.string());
  const std::filesystem::path fixtureFile =
      fixturesDir / "mode_shared_fixture.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WriteArchive(fixtureFile);
  const std::filesystem::path fixtureFileNew =
      fixturesDir / "mode_shared_fixture_new.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WriteArchive(fixtureFileNew);

  nlohmann::json entries = nlohmann::json::object();
  entries["TypeA"] = {
      {"file", fixtureFile.string()}, {"mode", "Mode A"}, {"color", "#000001"}};
  entries["TypeB"] = {
      {"file", fixtureFile.string()}, {"mode", "Mode_B"}, {"color", "#000002"}};
  entries["TypeC"] = {
      {"file", fixtureFile.string()}, {"mode", "mode-a"}, {"color", "#000003"}};
  WriteFile(
      dictPath,
            DictionaryJsonContract::MakeRoot("fixtures", std::move(entries)).dump(4));

  GdtfDictionary::UpdateVisualColorForFile("TypeA", fixtureFile.string(),
                                           "MODE-A", "#ABCDEF");

  auto dictAfterExplicitModeOpt = GdtfDictionary::Load();
  assert(dictAfterExplicitModeOpt.has_value());
  const auto &dictAfterExplicitMode = *dictAfterExplicitModeOpt;
  assert(dictAfterExplicitMode.at("TypeA").visualColorHex == "#ABCDEF");
  assert(dictAfterExplicitMode.at("TypeC").visualColorHex == "#ABCDEF");
  assert(dictAfterExplicitMode.at("TypeB").visualColorHex == "#000002");

  GdtfDictionary::UpdateVisualColorForFile("TypeA", fixtureFile.string(), {},
                                           "#FEDCBA");

  auto dictAfterEntryModeOpt = GdtfDictionary::Load();
  assert(dictAfterEntryModeOpt.has_value());
  const auto &dictAfterEntryMode = *dictAfterEntryModeOpt;
  assert(dictAfterEntryMode.at("TypeA").visualColorHex == "#FEDCBA");
  assert(dictAfterEntryMode.at("TypeC").visualColorHex == "#FEDCBA");
  assert(dictAfterEntryMode.at("TypeB").visualColorHex == "#000002");

  GdtfDictionary::UpdateVisualColorForFile(
      "TypeNotInDictionary", fixtureFile.string(), "mode_a", "#123456");

  auto dictAfterNewTypeOpt = GdtfDictionary::Load();
  assert(dictAfterNewTypeOpt.has_value());
  const auto &dictAfterNewType = *dictAfterNewTypeOpt;
  assert(dictAfterNewType.at("TypeA").visualColorHex == "#123456");
  assert(dictAfterNewType.at("TypeC").visualColorHex == "#123456");
  assert(dictAfterNewType.at("TypeB").visualColorHex == "#000002");
  assert(dictAfterNewType.at("TypeNotInDictionary").visualColorHex ==
         "#123456");

  nlohmann::json migratedEntries = nlohmann::json::object();
  migratedEntries["TypeA"] = {
      {"file", fixtureFile.string()}, {"mode", "Mode A"}, {"color", "#100000"}};
  migratedEntries["TypeB"] = {
      {"file", fixtureFile.string()}, {"mode", "Mode_B"}, {"color", "#200000"}};
  migratedEntries["TypeC"] = {{"file", fixtureFileNew.string()},
                              {"mode", "Mode A"},
                              {"color", "#300000"}};
  WriteFile(dictPath, DictionaryJsonContract::MakeRoot(
                          "fixtures", std::move(migratedEntries))
                         .dump(4));

  GdtfDictionary::UpdateVisualColorForFile("TypeA", fixtureFileNew.string(),
                                           "Mode A", "#654321");
  auto dictAfterPathRefreshOpt = GdtfDictionary::Load();
  assert(dictAfterPathRefreshOpt.has_value());
  const auto &dictAfterPathRefresh = *dictAfterPathRefreshOpt;
  assert(dictAfterPathRefresh.at("TypeA").path == fixtureFileNew.string());
  assert(dictAfterPathRefresh.at("TypeA").visualColorHex == "#654321");
  assert(dictAfterPathRefresh.at("TypeC").visualColorHex == "#654321");
  assert(dictAfterPathRefresh.at("TypeB").visualColorHex == "#200000");

  nlohmann::json inconsistentEntries = nlohmann::json::object();
  inconsistentEntries["TypeWithColor"] = {{"file", fixtureFileNew.string()},
                                          {"mode", "Mode A"},
                                          {"color", "#0F0F0F"}};
  inconsistentEntries["TypeWithoutColor"] = {{"file", fixtureFileNew.string()},
                                             {"mode", "mode-a"}};
  inconsistentEntries["TypeOtherMode"] = {{"file", fixtureFileNew.string()},
                                          {"mode", "Mode B"}};
  WriteFile(dictPath, DictionaryJsonContract::MakeRoot(
                          "fixtures", std::move(inconsistentEntries))
                .dump(4));

  auto dictAfterLoadHarmonizationOpt = GdtfDictionary::Load();
  assert(dictAfterLoadHarmonizationOpt.has_value());
  const auto &dictAfterLoadHarmonization = *dictAfterLoadHarmonizationOpt;
  assert(dictAfterLoadHarmonization.at("TypeWithColor").visualColorHex ==
         "#0F0F0F");
  assert(dictAfterLoadHarmonization.at("TypeWithoutColor").visualColorHex ==
         "#0F0F0F");
  assert(dictAfterLoadHarmonization.at("TypeOtherMode").visualColorHex.empty());

  std::filesystem::remove(fixtureFileNew);
  std::filesystem::remove(fixtureFile);
  std::filesystem::remove_all(isolatedRoot);

  return 0;
}
