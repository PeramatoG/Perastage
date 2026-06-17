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
  assert(fixtureTypeMap->FirstChildElement("FixtureTypeInfo") != nullptr);
  assert(fixtureTypeMap->FirstChildElement("FixtureTypeInfo")->NextSiblingElement("FixtureTypeInfo") == nullptr);

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
  assert(loaded.at("22222222-2222-2222-2222-222222222222").category == "Spot");

  // Dictionary should be updated with scene (MVR) priority category.
  auto entry = GdtfDictionary::Get("FixtureType");
  assert(entry.has_value());
  assert(entry->category == "Spot");
  auto secondEntry = GdtfDictionary::Get("FixtureType2");
  assert(secondEntry.has_value());
  assert(secondEntry->category == "Spot");

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
