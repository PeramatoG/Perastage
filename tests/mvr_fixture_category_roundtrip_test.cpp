/*
 * This file is part of Perastage.
 */
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <tinyxml2.h>
#include <wx/init.h>
#include <wx/mstream.h>
#include <wx/utils.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "configmanager.h"
#include "fixture.h"
#include "gdtf_test_fixture_builder.h"
#include "gdtfdictionary.h"
#include "fixture_visual_color.h"
#include "layer.h"
#include "mvrexporter.h"
#include "mvrimporter.h"
#include "wx_path_utils.h"

namespace fs = std::filesystem;

struct ArchiveEntry {
  std::string name;
  std::string bytes;
};

// Reads every root entry from a ZIP package without discarding duplicate names.
static std::vector<ArchiveEntry> ReadArchive(const fs::path &path) {
  wxFileInputStream input(WxPathUtils::WxStringFromFilesystemPath(path));
  wxZipInputStream zip(input);
  std::vector<ArchiveEntry> entries;
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry()), entry)) {
    ArchiveEntry result{entry->GetName().ToStdString(), {}};
    char buffer[4096];
    while (zip.CanRead()) {
      zip.Read(buffer, sizeof(buffer));
      const size_t read = zip.LastRead();
      if (read == 0)
        break;
      result.bytes.append(buffer, read);
    }
    entries.push_back(std::move(result));
  }
  return entries;
}

// Returns the bytes of one uniquely named package entry.
static std::string FindUniqueEntry(const std::vector<ArchiveEntry> &entries,
                                   const std::string &name) {
  std::string bytes;
  int count = 0;
  for (const auto &entry : entries) {
    if (entry.name == name) {
      bytes = entry.bytes;
      ++count;
    }
  }
  assert(count == 1);
  return bytes;
}

// Reads a binary file into a string for embedding in a compatibility package.
static std::string ReadFileBytes(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input), {});
}

// Reads description.xml from embedded GDTF archive bytes.
static std::string ReadGdtfDescription(const std::string &archiveBytes) {
  wxMemoryInputStream input(archiveBytes.data(), archiveBytes.size());
  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry()), entry)) {
    if (entry->GetName() != "description.xml")
      continue;
    std::string xml;
    char buffer[4096];
    while (zip.CanRead()) {
      zip.Read(buffer, sizeof(buffer));
      const size_t read = zip.LastRead();
      if (read == 0)
        break;
      xml.append(buffer, read);
    }
    return xml;
  }
  return {};
}

// Verifies that a fixture's GDTF reference names one package resource and mode.
static void VerifyFixtureGdtfReference(const tinyxml2::XMLElement *fixture,
                                       const std::vector<ArchiveEntry> &entries) {
  const auto *spec = fixture->FirstChildElement("GDTFSpec");
  const auto *mode = fixture->FirstChildElement("GDTFMode");
  assert(spec && spec->GetText());
  assert(mode && mode->GetText());
  const std::string description =
      ReadGdtfDescription(FindUniqueEntry(entries, spec->GetText()));
  tinyxml2::XMLDocument document;
  assert(document.Parse(description.c_str()) == tinyxml2::XML_SUCCESS);
  const auto *fixtureType = document.FirstChildElement("GDTF")->FirstChildElement("FixtureType");
  const auto *dmxModes = fixtureType->FirstChildElement("DMXModes");
  bool foundMode = false;
  for (const auto *dmxMode = dmxModes->FirstChildElement("DMXMode"); dmxMode;
       dmxMode = dmxMode->NextSiblingElement("DMXMode")) {
    foundMode = foundMode ||
                (dmxMode->Attribute("Name") &&
                 std::string(dmxMode->Attribute("Name")) == mode->GetText());
  }
  assert(foundMode);
}

// Writes a compatibility MVR whose XML uses an old GDTF name remapped to a real archive.
static void WriteCanonicalGdtfNameMismatchMvr(
    const fs::path &mvrPath, const std::string &xmlGdtfSpec,
    const std::string &archiveGdtfName, const std::string &gdtfBytes) {
  wxFileOutputStream output(WxPathUtils::WxStringFromFilesystemPath(mvrPath));
  assert(output.IsOk());
  wxZipOutputStream zip(output);
  auto writeEntry = [&](const std::string &entryName, const std::string &content) {
    auto *entry = new wxZipEntry(entryName);
    entry->SetMethod(wxZIP_METHOD_DEFLATE);
    assert(zip.PutNextEntry(entry));
    zip.Write(content.data(), content.size());
    assert(zip.CloseEntry());
  };

  const std::string xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" provider=\"Perastage\" providerVersion=\"test\">"
      "<Scene><Layers><Layer uuid=\"aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa\" name=\"Layer1\"><ChildList>"
      "<Fixture uuid=\"33333333-3333-4333-8333-333333333333\" name=\"Fixture\">"
      "<Matrix>1,0,0,0,1,0,0,0,1,0,0,0</Matrix><GDTFSpec>" +
      xmlGdtfSpec +
      "</GDTFSpec><GDTFMode>ModeA</GDTFMode><FixtureID>1</FixtureID>"
      "<FixtureIDNumeric>1</FixtureIDNumeric></Fixture>"
      "</ChildList></Layer></Layers></Scene></GeneralSceneDescription>";
  writeEntry("GeneralSceneDescription.xml", xml);
  writeEntry(archiveGdtfName, gdtfBytes);
  assert(zip.Close());
}

// Returns true when a fixture's direct XML children follow the expected MVR order.
static bool FixtureChildrenHaveExpectedOrder(const tinyxml2::XMLElement *fixture) {
  int lastIndex = -1;
  for (auto *child = fixture->FirstChildElement(); child;
       child = child->NextSiblingElement()) {
    const std::string name = child->Name();
    int index = -1;
    if (name == "Matrix") index = 0;
    else if (name == "GDTFSpec") index = 1;
    else if (name == "GDTFMode") index = 2;
    else if (name == "Position") index = 3;
    else if (name == "FixtureID") index = 4;
    else if (name == "FixtureIDNumeric") index = 5;
    else if (name == "UnitNumber") index = 6;
    else if (name == "Addresses") index = 7;
    else return false;
    if (index < lastIndex)
      return false;
    lastIndex = index;
  }
  return true;
}

// Verifies canonical fixture category and GDTF reference round trips.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const fs::path tempDir = fs::temp_directory_path() /
                           "mvr_fixture_category_roundtrip";
  fs::remove_all(tempDir);
  fs::create_directories(tempDir);
  const fs::path isolatedLibrary = tempDir / "library";
  fs::create_directories(isolatedLibrary / "fixtures");
  assert(wxSetEnv("PERASTAGE_LIBRARY_PATH",
                  WxPathUtils::WxStringFromFilesystemPath(isolatedLibrary)));

  const fs::path fixturePath = tempDir / "fixture.gdtf";
  tests::gdtf::BuildMinimalValidFixture()
      .WithDmxMode("ModeA", "Root")
      .WriteArchive(fixturePath);

  auto &cfg = ConfigManager::Get();
  cfg.Reset();
  MvrScene &scene = cfg.GetScene();
  scene.basePath = tempDir.string();

  Layer layer;
  layer.uuid = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
  layer.name = "Layer1";
  scene.layers[layer.uuid] = layer;

  Fixture fixture;
  fixture.uuid = "11111111-1111-4111-8111-111111111111";
  fixture.instanceName = "Fixture One";
  fixture.layer = layer.name;
  fixture.typeName = "FixtureType";
  fixture.gdtfSpec = fixturePath.string();
  fixture.gdtfMode = "ModeA";
  fixture.category = "Spot";
  fixture.categorySource = "Manual";
  fixture.visualColorHex = "#336699";
  scene.fixtures[fixture.uuid] = fixture;

  Fixture fixtureTwo = fixture;
  fixtureTwo.uuid = "22222222-2222-4222-8222-222222222222";
  fixtureTwo.instanceName = "Fixture Two";
  fixtureTwo.typeName = "FixtureType2";
  scene.fixtures[fixtureTwo.uuid] = fixtureTwo;

  GdtfDictionary::Update("FixtureType", fixturePath.string(), "ModeA", "Wash");
  GdtfDictionary::Update("FixtureType2", fixturePath.string(), "ModeA", "Beam");

  const fs::path mvrPath = tempDir / "fixture_category.mvr";
  MvrExporter exporter;
  assert(exporter.ExportToFile(mvrPath.string()));

  const auto entries = ReadArchive(mvrPath);
  const std::string sceneXmlText =
      FindUniqueEntry(entries, "GeneralSceneDescription.xml");
  assert(sceneXmlText.find("CategoryReason") == std::string::npos);
  tinyxml2::XMLDocument xml;
  assert(xml.Parse(sceneXmlText.c_str()) == tinyxml2::XML_SUCCESS);
  auto *root = xml.FirstChildElement("GeneralSceneDescription");
  auto *rootUserData = root->FirstChildElement("UserData");
  assert(rootUserData);
  auto *fixtureTypeMap = rootUserData->FirstChildElement("Data")
                             ->FirstChildElement("FixtureTypeInfoMap");
  assert(fixtureTypeMap);
  auto *fixtureTypeInfo = fixtureTypeMap->FirstChildElement("FixtureTypeInfo");
  assert(fixtureTypeInfo);
  assert(fixtureTypeInfo->NextSiblingElement("FixtureTypeInfo") == nullptr);
  assert(std::string(fixtureTypeInfo->FirstChildElement("Category")->GetText()) ==
         "Spot");
  assert(std::string(fixtureTypeInfo->FirstChildElement("CategorySource")->GetText()) ==
         "Manual");
  assert(std::string(fixtureTypeInfo->FirstChildElement("VisualColor")->GetText()) ==
         "#336699");

  int exportedFixtureCount = 0;
  for (auto *layerNode = root->FirstChildElement("Scene")
                             ->FirstChildElement("Layers")
                             ->FirstChildElement("Layer");
       layerNode; layerNode = layerNode->NextSiblingElement("Layer")) {
    auto *childList = layerNode->FirstChildElement("ChildList");
    if (!childList)
      continue;
    for (auto *fixtureNode = childList->FirstChildElement("Fixture"); fixtureNode;
         fixtureNode = fixtureNode->NextSiblingElement("Fixture")) {
      ++exportedFixtureCount;
      assert(fixtureNode->Attribute("uuid"));
      assert(fixtureNode->Attribute("name"));
      assert(fixtureNode->FirstChildElement("UserData") == nullptr);
      assert(fixtureNode->FirstChildElement("UnitNumber"));
      assert(FixtureChildrenHaveExpectedOrder(fixtureNode));
      VerifyFixtureGdtfReference(fixtureNode, entries);
    }
  }
  assert(exportedFixtureCount == 2);

  cfg.Reset();
  GdtfDictionary::ResetSaveCallCountForTesting();
  MvrImporter importer;
  assert(importer.ImportFromFile(mvrPath.string(), false, false));
  assert(GdtfDictionary::GetSaveCallCountForTesting() == 1);

  const auto verifyImported = [&](const MvrScene &imported) {
    assert(imported.fixtures.size() == 2);
    for (const auto &uuid : {fixture.uuid, fixtureTwo.uuid}) {
      const Fixture &loaded = imported.fixtures.at(uuid);
      assert(loaded.category == "Spot");
      assert(loaded.categorySource == "Manual");
      assert(loaded.visualColorHex == "#336699");
      assert(loaded.visualColorState == FixtureProjectColorState::Present);
      assert(loaded.automaticVisualColorHex == "#336699");
      assert(ResolveFixturePresentationColor(loaded).colorHex == "#336699");
      assert(!loaded.gdtfSpec.empty());
      assert(loaded.gdtfMode == "ModeA");
    }
  };
  verifyImported(cfg.GetScene());

  const fs::path secondMvrPath = tempDir / "fixture_category_second.mvr";
  assert(exporter.ExportToFile(secondMvrPath.string()));
  const auto secondEntries = ReadArchive(secondMvrPath);
  const std::string secondSceneXml =
      FindUniqueEntry(secondEntries, "GeneralSceneDescription.xml");
  tinyxml2::XMLDocument secondXml;
  assert(secondXml.Parse(secondSceneXml.c_str()) == tinyxml2::XML_SUCCESS);
  for (auto *fixtureNode = secondXml.FirstChildElement("GeneralSceneDescription")
                               ->FirstChildElement("Scene")
                               ->FirstChildElement("Layers")
                               ->FirstChildElement("Layer")
                               ->FirstChildElement("ChildList")
                               ->FirstChildElement("Fixture");
       fixtureNode; fixtureNode = fixtureNode->NextSiblingElement("Fixture")) {
    VerifyFixtureGdtfReference(fixtureNode, secondEntries);
  }
  cfg.Reset();
  assert(importer.ImportFromFile(secondMvrPath.string(), false, false));
  verifyImported(cfg.GetScene());

  const fs::path canonicalMismatchMvrPath =
      tempDir / "canonical_gdtf_name_mismatch.mvr";
  const std::string canonicalName = "Generic@Generic_1ch@Perastage.gdtf";
  WriteCanonicalGdtfNameMismatchMvr(
      canonicalMismatchMvrPath, "Generic 1ch.gdtf", canonicalName,
      ReadFileBytes(fixturePath));
  MvrImportResult mismatchResult;
  MvrImportOptions mismatchOptions;
  mismatchOptions.promptConflicts = false;
  mismatchOptions.applyDictionary = false;
  mismatchOptions.preserveMvrGdtfReferences = true;
  assert(importer.ImportFromFile(canonicalMismatchMvrPath.string(), mismatchResult,
                                 MvrImportMode::ParseOnly, mismatchOptions));
  const auto &mismatchFixture = mismatchResult.scene.fixtures.at(
      "33333333-3333-4333-8333-333333333333");
  assert(fs::path(mismatchFixture.gdtfSpec).filename() == canonicalName);
  assert(mismatchFixture.gdtfMode == "ModeA");

  cfg.Reset();
  MvrScene &sceneWithoutCategory = cfg.GetScene();
  sceneWithoutCategory.basePath = tempDir.string();
  sceneWithoutCategory.layers[layer.uuid] = layer;
  fixture.category.clear();
  fixture.categorySource.clear();
  fixture.categorySourceReason.clear();
  fixtureTwo.category.clear();
  fixtureTwo.categorySource.clear();
  fixtureTwo.categorySourceReason.clear();
  sceneWithoutCategory.fixtures[fixture.uuid] = fixture;
  sceneWithoutCategory.fixtures[fixtureTwo.uuid] = fixtureTwo;
  const fs::path noCategoryMvrPath = tempDir / "fixture_no_category.mvr";
  assert(exporter.ExportToFile(noCategoryMvrPath.string()));
  cfg.Reset();
  assert(importer.ImportFromFile(noCategoryMvrPath.string(), false, false));
  for (const auto &uuid : {fixture.uuid, fixtureTwo.uuid}) {
    const Fixture &loaded = cfg.GetScene().fixtures.at(uuid);
    assert(loaded.category == "Unknown");
    assert(loaded.categorySource == "AutoFallback");
    assert(!loaded.categorySourceReason.empty());
  }

  cfg.Reset();
  fs::remove_all(tempDir);
  return 0;
}
