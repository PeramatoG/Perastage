/*
 * This file is part of Perastage.
 */
#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <wx/init.h>

#include "dictionary_json_contract.h"
#include "gdtfdictionary.h"
#include "json.hpp"
#include "projectutils.h"

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

} // namespace

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const std::filesystem::path fixturesDir =
      std::filesystem::u8path(ProjectUtils::GetDefaultLibraryPath("fixtures"));
  std::filesystem::create_directories(fixturesDir);
  const std::filesystem::path dictPath = fixturesDir / "gdtf_dictionary.json";
  const bool hadOriginal = std::filesystem::exists(dictPath);
  const std::string originalContent = hadOriginal ? ReadFile(dictPath) : std::string{};

  const std::filesystem::path fixtureFile = fixturesDir / "mode_shared_fixture.gdtf";
  WriteFile(fixtureFile, "fixture");
  const std::filesystem::path fixtureFileNew = fixturesDir / "mode_shared_fixture_new.gdtf";
  WriteFile(fixtureFileNew, "fixture-new");

  nlohmann::json entries = nlohmann::json::object();
  entries["TypeA"] = {{"file", fixtureFile.string()},
                      {"mode", "Mode A"},
                      {"color", "#000001"}};
  entries["TypeB"] = {{"file", fixtureFile.string()},
                      {"mode", "Mode_B"},
                      {"color", "#000002"}};
  entries["TypeC"] = {{"file", fixtureFile.string()},
                      {"mode", "mode-a"},
                      {"color", "#000003"}};
  WriteFile(dictPath,
            DictionaryJsonContract::MakeRoot("fixtures", std::move(entries)).dump(4));

  GdtfDictionary::UpdateColorForFile("TypeA", fixtureFile.string(), "MODE-A", "#ABCDEF");

  auto dictAfterExplicitModeOpt = GdtfDictionary::Load();
  assert(dictAfterExplicitModeOpt.has_value());
  const auto &dictAfterExplicitMode = *dictAfterExplicitModeOpt;
  assert(dictAfterExplicitMode.at("TypeA").color == "#ABCDEF");
  assert(dictAfterExplicitMode.at("TypeC").color == "#ABCDEF");
  assert(dictAfterExplicitMode.at("TypeB").color == "#000002");

  GdtfDictionary::UpdateColorForFile("TypeA", fixtureFile.string(), {}, "#FEDCBA");

  auto dictAfterEntryModeOpt = GdtfDictionary::Load();
  assert(dictAfterEntryModeOpt.has_value());
  const auto &dictAfterEntryMode = *dictAfterEntryModeOpt;
  assert(dictAfterEntryMode.at("TypeA").color == "#FEDCBA");
  assert(dictAfterEntryMode.at("TypeC").color == "#FEDCBA");
  assert(dictAfterEntryMode.at("TypeB").color == "#000002");

  GdtfDictionary::UpdateColorForFile("TypeNotInDictionary", fixtureFile.string(),
                                     "mode_a", "#123456");

  auto dictAfterNewTypeOpt = GdtfDictionary::Load();
  assert(dictAfterNewTypeOpt.has_value());
  const auto &dictAfterNewType = *dictAfterNewTypeOpt;
  assert(dictAfterNewType.at("TypeA").color == "#123456");
  assert(dictAfterNewType.at("TypeC").color == "#123456");
  assert(dictAfterNewType.at("TypeB").color == "#000002");
  assert(dictAfterNewType.at("TypeNotInDictionary").color == "#123456");

  nlohmann::json migratedEntries = nlohmann::json::object();
  migratedEntries["TypeA"] = {{"file", fixtureFile.string()},
                              {"mode", "Mode A"},
                              {"color", "#100000"}};
  migratedEntries["TypeB"] = {{"file", fixtureFile.string()},
                              {"mode", "Mode_B"},
                              {"color", "#200000"}};
  migratedEntries["TypeC"] = {{"file", fixtureFileNew.string()},
                              {"mode", "Mode A"},
                              {"color", "#300000"}};
  WriteFile(dictPath, DictionaryJsonContract::MakeRoot("fixtures", std::move(migratedEntries))
                         .dump(4));

  GdtfDictionary::UpdateColorForFile("TypeA", fixtureFileNew.string(), "Mode A",
                                     "#654321");
  auto dictAfterPathRefreshOpt = GdtfDictionary::Load();
  assert(dictAfterPathRefreshOpt.has_value());
  const auto &dictAfterPathRefresh = *dictAfterPathRefreshOpt;
  assert(dictAfterPathRefresh.at("TypeA").path == fixtureFileNew.string());
  assert(dictAfterPathRefresh.at("TypeA").color == "#654321");
  assert(dictAfterPathRefresh.at("TypeC").color == "#654321");
  assert(dictAfterPathRefresh.at("TypeB").color == "#200000");

  std::filesystem::remove(fixtureFileNew);
  std::filesystem::remove(fixtureFile);
  if (hadOriginal)
    WriteFile(dictPath, originalContent);
  else
    std::filesystem::remove(dictPath);

  return 0;
}
