/*
 * This file is part of Perastage.
 */
#include "wx_path_utils.h"
#include "filesystem_path_utils.h"
#include <cassert>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

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

// Reads description.xml from a GDTF archive.
std::string ReadDescriptionXml(const std::filesystem::path &path) {
  wxFileInputStream input(WxPathUtils::WxStringFromFilesystemPath(path));
  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    if (entry->GetName() != "description.xml")
      continue;
    std::ostringstream bytes;
    char buffer[4096];
    while (true) {
      zip.Read(buffer, sizeof(buffer));
      const std::size_t count = zip.LastRead();
      if (count == 0)
        break;
      bytes.write(buffer, static_cast<std::streamsize>(count));
    }
    return bytes.str();
  }
  return {};
}

void WriteFile(const std::filesystem::path &path, const std::string &content) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  assert(out.is_open());
  out << content;
  assert(out.good());
}

// Writes a GDTF archive with description.xml stored below a root folder.
void WriteNestedDescriptionGdtf(const std::filesystem::path &path) {
  wxFileOutputStream output(WxPathUtils::WxStringFromFilesystemPath(path));
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

  const std::filesystem::path fixtureFile = fixturesDir / "color_fixture.gdtf";
  const std::filesystem::path unknownFixtureFile = fixturesDir / "mystery_fixture.gdtf";
  const std::filesystem::path nestedDummyFixtureFile =
      fixturesDir / "Dummy 1ch.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WriteArchive(fixtureFile);
  const std::filesystem::path externalFixtureFile =
      isolatedRoot / "downloads" / "GLP@JDC1.gdtf";
  std::filesystem::create_directories(externalFixtureFile.parent_path());
  tests::gdtf::BuildMinimalValidFixture().WriteArchive(externalFixtureFile);
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

  const auto externalMapping =
      GdtfDictionary::CreateOrUpdateExternalLibraryMapping(
          "GLP JDC1", externalFixtureFile.string(), "ModeA");
  assert(externalMapping.success);
  assert(externalMapping.entry.mode == "ModeA");
  assert(std::filesystem::exists(externalMapping.entry.path));
  assert(std::filesystem::path(externalMapping.entry.path).filename() ==
         "GLP@JDC1.gdtf");
  assert(!GdtfDictionary::IsPerastageNamedGdtfFile(
      externalMapping.entry.path));

  const std::filesystem::path legacyDerivativeSource =
      isolatedRoot / "downloads" / "LegacyFixture.gdtf";
  tests::gdtf::BuildMinimalValidFixture()
      .WithFixtureIdentity("LegacyFixture", "Acme",
                           tests::gdtf::FixtureBuilder::kMinimalFixtureTypeId)
      .WithModelResource("main")
      .WithPerastageGeneratedSymbols()
      .WithEditor("perastage/1.5")
      .WriteArchive(legacyDerivativeSource);
  const std::string legacySourceBytes = ReadFile(legacyDerivativeSource);
  const auto legacyDerivative =
      GdtfDictionary::CreateOrUpdatePerastageLibraryDerivative(
          "Legacy Fixture", legacyDerivativeSource.string(), "Default");
  assert(legacyDerivative.has_value());
  assert(ReadFile(legacyDerivativeSource) == legacySourceBytes);
  const std::string derivativeXml = ReadDescriptionXml(legacyDerivative->path);
  assert(derivativeXml.find("Editor=") == std::string::npos);
  assert(derivativeXml.find("PerastageMutationAudit") == std::string::npos);

  const std::filesystem::path unknownDerivativeSource =
      isolatedRoot / "downloads" / "UnknownFixture.gdtf";
  tests::gdtf::BuildMinimalValidFixture()
      .WithFixtureIdentity("UnknownFixture", "Acme",
                           tests::gdtf::FixtureBuilder::kMinimalFixtureTypeId)
      .WithModelResource("main")
      .WithPerastageGeneratedSymbols()
      .WithFixtureTypeExtensionAttribute("VendorData", "keep")
      .WriteArchive(unknownDerivativeSource);
  const std::string unknownSourceBytes = ReadFile(unknownDerivativeSource);
  assert(!GdtfDictionary::CreateOrUpdatePerastageLibraryDerivative(
              "Unknown Fixture", unknownDerivativeSource.string(), "Default")
              .has_value());
  assert(ReadFile(unknownDerivativeSource) == unknownSourceBytes);
  assert(!std::filesystem::exists(fixturesDir /
                                  "Acme@UnknownFixture@Perastage.gdtf"));

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

  std::filesystem::remove_all(isolatedRoot);

  return 0;
}
