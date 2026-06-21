/*
 * This file is part of Perastage.
 */
#include <cassert>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include <tinyxml2.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include <wx/init.h>

#include "configmanager.h"
#include "fixture.h"
#include "gdtfdictionary.h"
#include "layer.h"
#include "mvrexporter.h"
#include "mvrimporter.h"
#include "projectutils.h"

// Reads GeneralSceneDescription.xml from an exported MVR package.
static std::string ReadSceneXml(const std::filesystem::path &mvrPath) {
  wxFileInputStream input(mvrPath.string());
  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry()), entry)) {
    if (entry->GetName() != "GeneralSceneDescription.xml")
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

// Writes a minimal MVR whose XML GDTFSpec uses an old name while the package uses a canonical name.
static void WriteCanonicalGdtfNameMismatchMvr(
    const std::filesystem::path &mvrPath, const std::string &xmlGdtfSpec,
    const std::string &archiveGdtfName) {
  wxFileOutputStream output(mvrPath.string());
  assert(output.IsOk());
  wxZipOutputStream zip(output);
  auto writeEntry = [&](const std::string &entryName,
                        const std::string &content) {
    auto *entry = new wxZipEntry(entryName);
    entry->SetMethod(wxZIP_METHOD_DEFLATE);
    assert(zip.PutNextEntry(entry));
    zip.Write(content.c_str(), content.size());
    assert(zip.CloseEntry());
  };

  const std::string xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" "
      "provider=\"Perastage\" providerVersion=\"test\">"
      "<Scene><Layers><Layer uuid=\"layer1\" name=\"Layer1\"><ChildList>"
      "<Fixture uuid=\"33333333-3333-3333-3333-333333333333\" "
      "name=\"Fixture\"><Matrix>1,0,0,0,1,0,0,0,1,0,0,0</Matrix><GDTFSpec>" +
      xmlGdtfSpec +
      "</GDTFSpec><GDTFMode>Default</GDTFMode><FixtureID>1</FixtureID>"
      "<FixtureIDNumeric>1</FixtureIDNumeric></Fixture>"
      "</ChildList></Layer></Layers></Scene></GeneralSceneDescription>";
  writeEntry("GeneralSceneDescription.xml", xml);
  writeEntry(archiveGdtfName, "fixture");
  assert(zip.Close());
}

// Returns true when a fixture's direct XML children follow the expected MVR order.
static bool FixtureChildrenHaveExpectedOrder(tinyxml2::XMLElement *fixture) {
  int lastIndex = -1;
  for (tinyxml2::XMLElement *child = fixture->FirstChildElement(); child;
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

// Verifies fixture category metadata export/import compatibility.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  auto &cfg = ConfigManager::Get();
  cfg.Reset();
  MvrScene &scene = cfg.GetScene();

  Layer layer;
  layer.uuid = "layer1";
  layer.name = "Layer1";
  scene.layers[layer.uuid] = layer;

  const std::filesystem::path tempDir =
      std::filesystem::temp_directory_path() / "mvr_fixture_category_roundtrip";
  std::filesystem::remove_all(tempDir);
  std::filesystem::create_directories(tempDir);

  const std::filesystem::path fixturePath = tempDir / "fixture.gdtf";
  std::ofstream(fixturePath) << "fixture";
  scene.basePath = tempDir.string();

  Fixture fixture;
  fixture.uuid = "11111111-1111-1111-1111-111111111111";
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
  fixtureTwo.uuid = "22222222-2222-2222-2222-222222222222";
  fixtureTwo.instanceName = "Fixture Two";
  fixtureTwo.typeName = "FixtureType2";
  scene.fixtures[fixtureTwo.uuid] = fixtureTwo;

  GdtfDictionary::Update("FixtureType", fixturePath.string(), "ModeA", "Wash");
  GdtfDictionary::Update("FixtureType2", fixturePath.string(), "ModeA", "Beam");

  const std::filesystem::path mvrPath = tempDir / "fixture_category.mvr";
  MvrExporter exporter;
  assert(exporter.ExportToFile(mvrPath.string()));

  const std::string sceneXmlText = ReadSceneXml(mvrPath);
  assert(!sceneXmlText.empty());
  assert(sceneXmlText.find("CategoryReason") == std::string::npos);
  tinyxml2::XMLDocument xml;
  assert(xml.Parse(sceneXmlText.c_str()) == tinyxml2::XML_SUCCESS);
  auto *root = xml.FirstChildElement("GeneralSceneDescription");
  assert(root != nullptr);
  auto *rootUserData = root->FirstChildElement("UserData");
  assert(rootUserData != nullptr);
  auto *fixtureTypeMap = rootUserData->FirstChildElement("Data")->FirstChildElement("FixtureTypeInfoMap");
  assert(fixtureTypeMap != nullptr);
  auto *fixtureTypeInfo =
      fixtureTypeMap->FirstChildElement("FixtureTypeInfo");
  assert(fixtureTypeInfo != nullptr);
  assert(fixtureTypeInfo->NextSiblingElement("FixtureTypeInfo") == nullptr);
  assert(fixtureTypeInfo->FirstChildElement("VisualColor") != nullptr);
  assert(std::string(
             fixtureTypeInfo->FirstChildElement("VisualColor")->GetText()) ==
         "#336699");

  int exportedFixtureCount = 0;
  for (auto *layerNode = root->FirstChildElement("Scene")->FirstChildElement("Layers")->FirstChildElement("Layer");
       layerNode; layerNode = layerNode->NextSiblingElement("Layer")) {
    auto *childList = layerNode->FirstChildElement("ChildList");
    if (!childList)
      continue;
    for (auto *fixtureNode = childList->FirstChildElement("Fixture"); fixtureNode;
         fixtureNode = fixtureNode->NextSiblingElement("Fixture")) {
      ++exportedFixtureCount;
      assert(fixtureNode->Attribute("uuid") != nullptr);
      assert(fixtureNode->Attribute("name") != nullptr);
      assert(fixtureNode->FirstChildElement("UserData") == nullptr);
      assert(fixtureNode->FirstChildElement("UnitNumber") != nullptr);
      assert(FixtureChildrenHaveExpectedOrder(fixtureNode));
    }
  }
  assert(exportedFixtureCount == 2);

  cfg.Reset();
  GdtfDictionary::ResetSaveCallCountForTesting();
  MvrImporter importer;
  assert(importer.ImportFromFile(mvrPath.string(), false, false));
  assert(GdtfDictionary::GetSaveCallCountForTesting() == 1);

  const auto &loaded = cfg.GetScene().fixtures;
  assert(loaded.size() == 2);
  const Fixture &loadedFixture = loaded.at("11111111-1111-1111-1111-111111111111");
  assert(loadedFixture.category == "Spot");
  assert(loadedFixture.visualColorHex == "#336699");
  assert(loaded.at("22222222-2222-2222-2222-222222222222").category == "Spot");
  assert(loaded.at("22222222-2222-2222-2222-222222222222")
             .visualColorHex == "#336699");

  // Dictionary should be updated with scene (MVR) priority category.
  auto entry = GdtfDictionary::Get("FixtureType");
  assert(entry.has_value());
  assert(entry->category == "Spot");
  auto secondEntry = GdtfDictionary::Get("FixtureType2");
  assert(secondEntry.has_value());
  assert(secondEntry->category == "Spot");

  const std::filesystem::path canonicalMismatchMvrPath =
      tempDir / "canonical_gdtf_name_mismatch.mvr";
  WriteCanonicalGdtfNameMismatchMvr(
      canonicalMismatchMvrPath, "Generic 1ch.gdtf",
      "Generic@Generic_1ch@Perastage.gdtf");
  MvrImportResult canonicalMismatchResult;
  MvrImportOptions canonicalMismatchOptions;
  canonicalMismatchOptions.promptConflicts = false;
  canonicalMismatchOptions.applyDictionary = false;
  canonicalMismatchOptions.preserveMvrGdtfReferences = true;
  assert(importer.ImportFromFile(canonicalMismatchMvrPath.string(),
                                 canonicalMismatchResult,
                                 MvrImportMode::ParseOnly,
                                 canonicalMismatchOptions));
  const auto &canonicalMismatchFixture =
      canonicalMismatchResult.scene.fixtures.at(
          "33333333-3333-3333-3333-333333333333");
  assert(std::filesystem::path(canonicalMismatchFixture.gdtfSpec).filename() ==
         "Generic@Generic_1ch@Perastage.gdtf");

  cfg.Reset();
  MvrScene &scene2 = cfg.GetScene();
  scene2.layers[layer.uuid] = layer;
  Fixture fixtureWithoutCategory = fixture;
  fixtureWithoutCategory.category.clear();
  fixtureWithoutCategory.categorySource.clear();
  scene2.fixtures[fixtureWithoutCategory.uuid] = fixtureWithoutCategory;
  Fixture fixtureTwoWithoutCategory = fixtureTwo;
  fixtureTwoWithoutCategory.category.clear();
  fixtureTwoWithoutCategory.categorySource.clear();
  scene2.fixtures[fixtureTwoWithoutCategory.uuid] = fixtureTwoWithoutCategory;

  const std::filesystem::path noCategoryMvrPath = tempDir / "fixture_no_category.mvr";
  assert(exporter.ExportToFile(noCategoryMvrPath.string()));

  cfg.Reset();
  assert(importer.ImportFromFile(noCategoryMvrPath.string(), false, false));
  const auto &loadedNoCategory = cfg.GetScene().fixtures;
  assert(loadedNoCategory.at("11111111-1111-1111-1111-111111111111").category == "Spot");
  assert(loadedNoCategory.at("22222222-2222-2222-2222-222222222222").category == "Spot");

  std::filesystem::remove_all(tempDir);
  std::filesystem::remove(ProjectUtils::GetDefaultLibraryPath("fixtures") +
                          "/fixture.gdtf");
  return 0;
}
