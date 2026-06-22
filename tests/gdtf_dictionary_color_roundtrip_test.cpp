/*
 * This file is part of Perastage.
 */
#include "filesystem_path_utils.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

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

// Writes a GDTF archive with description.xml stored below a root folder.
void WriteNestedDescriptionGdtf(const std::filesystem::path &path) {
  wxFileOutputStream output(path.string());
  assert(output.IsOk());
  wxZipOutputStream zip(output);
  auto *entry = new wxZipEntry("Dummy 1ch/description.xml");
  entry->SetMethod(wxZIP_METHOD_DEFLATE);
  assert(zip.PutNextEntry(entry));
  const std::string description =
      "<GDTF DataVersion=\"1.2\"><FixtureType Manufacturer=\"Perastage\" "
      "Name=\"Dummy 1ch\" /></GDTF>";
  zip.Write(description.c_str(), description.size());
  assert(zip.CloseEntry());
  assert(zip.Close());
}

} // namespace

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const std::filesystem::path fixturesDir =
      PathUtils::PathFromUtf8(ProjectUtils::GetDefaultLibraryPath("fixtures"));
  std::filesystem::create_directories(fixturesDir);
  const std::filesystem::path dictPath = fixturesDir / "gdtf_dictionary.json";

  const bool hadOriginal = std::filesystem::exists(dictPath);
  const std::string originalContent =
      hadOriginal ? ReadFile(dictPath) : std::string{};

  const std::filesystem::path fixtureFile = fixturesDir / "color_fixture.gdtf";
  const std::filesystem::path unknownFixtureFile = fixturesDir / "mystery_fixture.gdtf";
  const std::filesystem::path nestedDummyFixtureFile =
      fixturesDir / "Dummy 1ch.gdtf";
  WriteFile(fixtureFile, "fixture");
  WriteFile(unknownFixtureFile, "not a zip archive");
  WriteNestedDescriptionGdtf(nestedDummyFixtureFile);

  nlohmann::json entries = nlohmann::json::object();
  entries["ColorOnlyType"] = {{"color", "#112233"}};
  entries["FullType"] = {{"file", fixtureFile.string()},
                         {"mode", "ModeA"},
                         {"category", "Wash"},
                         {"color", "#ABCDEF"}};
  WriteFile(
      dictPath,
            DictionaryJsonContract::MakeRoot("fixtures", std::move(entries)).dump(4));

  auto loadedOpt = GdtfDictionary::Load();
  assert(loadedOpt.has_value());

  const auto colorOnlyIt = loadedOpt->find("ColorOnlyType");
  assert(colorOnlyIt != loadedOpt->end());
  assert(colorOnlyIt->second.visualColorHex == "#112233");
  assert(colorOnlyIt->second.path.empty());

  const auto fullIt = loadedOpt->find("FullType");
  assert(fullIt != loadedOpt->end());
  assert(fullIt->second.visualColorHex == "#ABCDEF");

  assert(GdtfDictionary::BuildPerastageCanonicalGdtfFileName(
             unknownFixtureFile.string()) ==
         "Unknown@mystery_fixture@Perastage.gdtf");
  assert(GdtfDictionary::BuildPerastageCanonicalGdtfFileName(
             nestedDummyFixtureFile.string()) ==
         "Perastage@Dummy_1ch@Perastage.gdtf");
  assert(GdtfDictionary::IsPerastageNamedGdtfFile(
      "Manufacturer@Fixture@Perastage.gdtf"));
  GdtfDictionary::Update("Some Type", nestedDummyFixtureFile.string(), "");
  auto dummyCanonicalPathEntry = GdtfDictionary::Get("Some Type");
  assert(!dummyCanonicalPathEntry.has_value());
  GdtfDictionary::Update(
      "Some Type", (fixturesDir / "Perastage@Dummy_1ch@Perastage.gdtf").string(),
      "");
  dummyCanonicalPathEntry = GdtfDictionary::Get("Some Type");
  assert(!dummyCanonicalPathEntry.has_value());

  assert(GdtfDictionary::Save(*loadedOpt));

  nlohmann::json savedRoot;
  {
    std::ifstream in(dictPath);
    assert(in.is_open());
    in >> savedRoot;
  }

  assert(savedRoot.contains("entries"));
  assert(savedRoot["entries"]["ColorOnlyType"]["visual_color"] == "#112233");
  assert(savedRoot["entries"]["FullType"]["visual_color"] == "#ABCDEF");
  assert(!savedRoot["entries"]["ColorOnlyType"].contains("color"));
  assert(!savedRoot["entries"]["FullType"].contains("color"));

  std::filesystem::remove(fixtureFile);
  std::filesystem::remove(unknownFixtureFile);
  std::filesystem::remove(nestedDummyFixtureFile);
  if (hadOriginal)
    WriteFile(dictPath, originalContent);
  else
    std::filesystem::remove(dictPath);

  return 0;
}
